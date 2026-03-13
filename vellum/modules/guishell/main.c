#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include <vellum/device.h>
#include <vellum/font.h>
#include <vellum/hid.h>
#include <vellum/interface/framebuffer.h>
#include <vellum/interface/hid.h>
#include <vellum/interface/video.h>
#include <vellum/macros.h>
#include <vellum/shell.h>

static struct device *fbdev;
static const struct video_interface *vidif;
static const struct framebuffer_interface *fbif;
static struct device *kbdev;
static const struct hid_interface *kbhidif;
static struct device *msdev;
static const struct hid_interface *mshidif;
static int current_vmode;
static struct video_mode_info vmode_info;
static uint32_t *framebuffer;

static const uint32_t color_palette[16] = {
    0x000000,
    0x000080,
    0x008000,
    0x008080,
    0x800000,
    0x800080,
    0x808000,
    0xC0C0C0,
    0x808080,
    0x0000FF,
    0x00FF00,
    0x00FFFF,
    0xFF0000,
    0xFF00FF,
    0xFFFF00,
    0xFFFFFF,
};

static void draw_line(int x0, int y0, int x1, int y1, uint32_t color)
{
    if (x0 == x1) {
        if (y0 > y1) {
            int temp = y0;
            y0 = y1;
            y1 = temp;
        }
        for (int y = y0; y <= y1; y++) {
            framebuffer[y * vmode_info.width + x0] = color;
        }
    } else if (y0 == y1) {
        if (x0 > x1) {
            int temp = x0;
            x0 = x1;
            x1 = temp;
        }
        for (int x = x0; x <= x1; x++) {
            framebuffer[y0 * vmode_info.width + x] = color;
        }
    } else {
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy, e2;

        for (;;) {
            framebuffer[y0 * vmode_info.width + x0] = color;
            if (x0 == x1 && y0 == y1) break;
            e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }
}

static inline uint32_t blend_color(uint32_t upper, uint32_t lower)
{
    uint32_t new_color;

    int alpha = (upper >> 24) & 0xFF;

    if (!alpha) {
        new_color = lower;
    } else if (alpha == 0xFF) {
        new_color = upper & 0xFFFFFF;
    } else {
        int upper_r = (upper >> 16) & 0xFF;
        int upper_g = (upper >> 8) & 0xFF;
        int upper_b = upper & 0xFF;
        int lower_r = (lower >> 16) & 0xFF;
        int lower_g = (lower >> 8) & 0xFF;
        int lower_b = lower & 0xFF;

        new_color = ((lower_r * (255 - alpha) / 255 + upper_r * alpha / 255) << 16) |
            ((lower_g * (255 - alpha) / 255 + upper_g * alpha / 255) << 8) |
            (lower_b * (255 - alpha) / 255 + upper_b * alpha / 255);
    }

    return new_color;
}

static void draw_rect(int xpos, int ypos, int width, int height, uint32_t color, int fill)
{
    for (int x = xpos; x < xpos + width; x++) {
        framebuffer[ypos * vmode_info.width + x] = color;
    }

    for (int y = ypos + 1; y < ypos + height - 1; y++) {
        if (fill) {
            for (int x = xpos; x < xpos + width; x++) {
                framebuffer[y * vmode_info.width + x] = color;
            }
        } else {
            framebuffer[y * vmode_info.width + xpos] = color;
            framebuffer[y * vmode_info.width + xpos + width - 1] = color;
        }
    }

    for (int x = xpos; x < xpos + width; x++) {
        framebuffer[(ypos + height - 1) * vmode_info.width + x] = color;
    }
}

void draw_circle(int x0, int y0, int radius, uint32_t color, int fill)
{
    int f = 1 - radius;
    int ddF_x = 1;
    int ddF_y = -2 * radius;
    int x = 0;
    int y = radius;

    if (fill) {
        draw_line(x0 + radius, y0, x0 - radius, y0, color);
    } else {
        framebuffer[y0 * vmode_info.width + x0 + radius] = color;
        framebuffer[y0 * vmode_info.width + x0 - radius] = color;
    }
    framebuffer[(y0 + radius) * vmode_info.width + x0] = color;
    framebuffer[(y0 - radius) * vmode_info.width + x0] = color;

    while (x < y) {
        // ddF_x == 2 * x + 1;
        // ddF_y == -2 * y;
        // f == x*x + y*y - radius*radius + 2*x - y + 1;
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        if (fill) {
            draw_line(x0 + x, y0 + y, x0 - x, y0 + y, color);
            draw_line(x0 + x, y0 - y, x0 - x, y0 - y, color);
            draw_line(x0 + y, y0 + x, x0 - y, y0 + x, color);
            draw_line(x0 + y, y0 - x, x0 - y, y0 - x, color);
        } else {
            framebuffer[(y0 + y) * vmode_info.width + x0 + x] = color;
            framebuffer[(y0 + y) * vmode_info.width + x0 - x] = color;
            framebuffer[(y0 - y) * vmode_info.width + x0 + x] = color;
            framebuffer[(y0 - y) * vmode_info.width + x0 - x] = color;
            framebuffer[(y0 + x) * vmode_info.width + x0 + y] = color;
            framebuffer[(y0 + x) * vmode_info.width + x0 - y] = color;
            framebuffer[(y0 - x) * vmode_info.width + x0 + y] = color;
            framebuffer[(y0 - x) * vmode_info.width + x0 - y] = color;
        }
    }
}

void draw_ellipse_rect(int x0, int y0, int x1, int y1, uint32_t color, int fill)
{
    int a = abs(x1 - x0), b = abs(y1 - y0), b1 = b & 1;       /* values of diameter */
    long dx = 4 * (1 - a) * b * b, dy = 4 * (b1 + 1) * a * a; /* error increment */
    long err = dx + dy + b1 * a * a, e2;                      /* error of 1.step */

    if (x0 > x1) {
        x0 = x1;
        x1 += a;
    } /* if called with swapped points */
    if (y0 > y1) {
        y0 = y1;
    } /* .. exchange them */
    y0 += (b + 1) / 2;
    y1 = y0 - b1; /* starting pixel */
    a *= 8 * a;
    b1 = 8 * b * b;
    do {
        if (fill) {
            draw_line(x0, y0, x1, y0, color);
            draw_line(x0, y1, x1, y1, color);
        } else {
            framebuffer[y0 * vmode_info.width + x1] = color;
            framebuffer[y0 * vmode_info.width + x0] = color;
            framebuffer[y1 * vmode_info.width + x0] = color;
            framebuffer[y1 * vmode_info.width + x1] = color;
        }
        e2 = 2 * err;
        if (e2 >= dx) {
            x0++;
            x1--;
            err += dx += b1;
        } /* x step */
        if (e2 <= dy) {
            y0++;
            y1--;
            err += dy += a;
        } /* y step */
    } while (x0 <= x1);

    while (y0 - y1 < b) { /* too early stop of flat ellipses a=1 */
        if (fill) {
            draw_line(x0 - 1, y0, x1 + 1, y0, color);
            y0++;
            draw_line(x0 - 1, y1, x1 + 1, y1, color);
            y1--;
        } else {
            framebuffer[y0 * vmode_info.width + x0 - 1] = color;
            framebuffer[y0++ * vmode_info.width + x1 + 1] = color;
            framebuffer[y1 * vmode_info.width + x0 - 1] = color;
            framebuffer[y1-- * vmode_info.width + x1 + 1] = color;
        }
    }
}

static void draw_bezier2_part(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color)
{
    int sx = x0 < x2 ? 1 : -1;
    int sy = y0 < y2 ? 1 : -1;                                           /* step direction */
    int cur = sx * sy * ((x0 - x1) * (y2 - y1) - (x2 - x1) * (y0 - y1)); /* curvature */
    int x = x0 - 2 * x1 + x2, y = y0 - 2 * y1 + y2, xy = 2 * x * y * sx * sy;
    /* compute error increments of P0 */
    long dx = (1 - 2 * abs(x0 - x1)) * y * y + abs(y0 - y1) * xy - 2 * cur * abs(y0 - y2);
    long dy = (1 - 2 * abs(y0 - y1)) * x * x + abs(x0 - x1) * xy + 2 * cur * abs(x0 - x2);
    /* compute error increments of P2 */
    long ex = (1 - 2 * abs(x2 - x1)) * y * y + abs(y2 - y1) * xy + 2 * cur * abs(y0 - y2);
    long ey = (1 - 2 * abs(y2 - y1)) * x * x + abs(x2 - x1) * xy - 2 * cur * abs(x0 - x2);
    /* sign of gradient must not change */
    assert((x0 - x1) * (x2 - x1) <= 0 && (y0 - y1) * (y2 - y1) <= 0);
    if (cur == 0) { /* straight line */
        draw_line(x0, y0, x2, y2, color);
        return;
    }
    x *= 2 * x;
    y *= 2 * y;
    if (cur < 0) { /* negated curvature */
        x = -x;
        dx = -dx;
        ex = -ex;
        xy = -xy;
        y = -y;
        dy = -dy;
        ey = -ey;
    }
    /* algorithm fails for almost straight line, check error values */
    if (dx >= -y || dy <= -x || ex <= -y || ey >= -x) {
        draw_line(x0, y0, x1, y1, color);
        draw_line(x1, y1, x2, y2, color);
        return;
    }
    dx -= xy;
    ex = dx + dy;
    dy -= xy;  /* error of 1.step */
    for (;;) { /* plot curve */
        framebuffer[y0 * vmode_info.width + x0] = color;
        ey = 2 * ex - dy;   /* save value for test of y step */
        if (2 * ex >= dx) { /* x step */
            if (x0 == x2) break;
            x0 += sx;
            dy -= xy;
            ex += dx += y;
        }
        if (ey <= 0) { /* y step */
            if (y0 == y2) break;
            y0 += sy;
            dx -= xy;
            ex += dy += x;
        }
    }
}
static inline int inside(int a, int b, int c)
{
    return (a <= b && b <= c) || (c <= b && b <= a);
}

static inline int lerp2(int a, int b)
{
    return (a + b) >> 1;
}

void draw_bezier2(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color)
{
    /* Zingl assert 조건 검사 */
    if (inside(x0, x1, x2) && inside(y0, y1, y2)) {
        draw_bezier2_part(x0, y0, x1, y1, x2, y2, color);
        return;
    }

    /* t = 1/2 분할 (De Casteljau) */
    int x01 = lerp2(x0, x1);
    int y01 = lerp2(y0, y1);
    int x12 = lerp2(x1, x2);
    int y12 = lerp2(y1, y2);

    int x012 = lerp2(x01, x12);
    int y012 = lerp2(y01, y12);

    draw_bezier2(x0, y0, x01, y01, x012, y012, color);
    draw_bezier2(x012, y012, x12, y12, x2, y2, color);
}

static inline int mid(int a, int b)
{
    return (a + b) >> 1;
}

static inline long cross(long ax, long ay, long bx, long by)
{
    return ax * by - ay * bx;
}

static void split_bezier3(
    int x0,
    int y0,
    int x1,
    int y1,
    int x2,
    int y2,
    int x3,
    int y3,
    int *lx0,
    int *ly0,
    int *lx1,
    int *ly1,
    int *lx2,
    int *ly2,
    int *lx3,
    int *ly3,
    int *rx0,
    int *ry0,
    int *rx1,
    int *ry1,
    int *rx2,
    int *ry2,
    int *rx3,
    int *ry3
)
{
    int x01 = mid(x0, x1), y01 = mid(y0, y1);
    int x12 = mid(x1, x2), y12 = mid(y1, y2);
    int x23 = mid(x2, x3), y23 = mid(y2, y3);

    int x012 = mid(x01, x12), y012 = mid(y01, y12);
    int x123 = mid(x12, x23), y123 = mid(y12, y23);

    int x0123 = mid(x012, x123), y0123 = mid(y012, y123);

    *lx0 = x0;
    *ly0 = y0;
    *lx1 = x01;
    *ly1 = y01;
    *lx2 = x012;
    *ly2 = y012;
    *lx3 = x0123;
    *ly3 = y0123;

    *rx0 = x0123;
    *ry0 = y0123;
    *rx1 = x123;
    *ry1 = y123;
    *rx2 = x23;
    *ry2 = y23;
    *rx3 = x3;
    *ry3 = y3;
}

static int bezier3_flat_enough(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3)
{
    long dx = x3 - x0;
    long dy = y3 - y0;

    long d1 = labs(cross(dx, dy, x1 - x0, y1 - y0));
    long d2 = labs(cross(dx, dy, x2 - x0, y2 - y0));

    /* tolerance = 1 pixel */
    return (d1 <= labs(dx) + labs(dy)) && (d2 <= labs(dx) + labs(dy));
}

#define BEZIER3_MAX_SPLIT 24

void draw_bezier3(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color)
{
    struct stack {
        int x0, y0, x1, y1, x2, y2, x3, y3;
        int depth;
    } stack[BEZIER3_MAX_SPLIT];

    int sp = 0;
    stack[sp++] = (struct stack){x0, y0, x1, y1, x2, y2, x3, y3, 0};

    while (sp > 0) {
        struct stack c = stack[--sp];

        /* 충분히 평평 → 2차로 근사 */
        if (bezier3_flat_enough(c.x0, c.y0, c.x1, c.y1, c.x2, c.y2, c.x3, c.y3)) {
            /* 3차 → 2차 근사 */
            int qx1 = (c.x1 * 3 + c.x2 * 3 - c.x0 - c.x3) >> 2;
            int qy1 = (c.y1 * 3 + c.y2 * 3 - c.y0 - c.y3) >> 2;

            draw_bezier2(c.x0, c.y0, qx1, qy1, c.x3, c.y3, color);
            continue;
        }

        /* 분할 한계 */
        if (c.depth >= BEZIER3_MAX_SPLIT - 1) {
            draw_line(c.x0, c.y0, c.x3, c.y3, color);
            continue;
        }

        /* 분할 */
        int lx0, ly0, lx1, ly1, lx2, ly2, lx3, ly3;
        int rx0, ry0, rx1, ry1, rx2, ry2, rx3, ry3;

        split_bezier3(
            c.x0,
            c.y0,
            c.x1,
            c.y1,
            c.x2,
            c.y2,
            c.x3,
            c.y3,
            &lx0,
            &ly0,
            &lx1,
            &ly1,
            &lx2,
            &ly2,
            &lx3,
            &ly3,
            &rx0,
            &ry0,
            &rx1,
            &ry1,
            &rx2,
            &ry2,
            &rx3,
            &ry3
        );

        /* 오른쪽 먼저 push → 왼쪽이 먼저 그려짐 */
        stack[sp++] = (typeof(stack[0])){rx0, ry0, rx1, ry1, rx2, ry2, rx3, ry3, c.depth + 1};
        stack[sp++] = (typeof(stack[0])){lx0, ly0, lx1, ly1, lx2, ly2, lx3, ly3, c.depth + 1};
    }
}

void draw_polygon(int pts[][2], int count, uint32_t color, int fill)
{
    if (count < 3) return;

    // 1. Wireframe 모드: Bresenham 기반 선 그리기만 수행
    if (!fill) {
        for (int i = 0; i < count; i++) {
            draw_line(
                pts[i][0],
                pts[i][1],
                pts[(i + 1) % count][0],
                pts[(i + 1) % count][1],
                color
            );
        }
        return;
    }

    // 2. Filled 모드: Scanline 알고리즘
    // 다각형의 Y축 범위(Bounding Box) 찾기
    int min_y = pts[0][1];
    int max_y = pts[0][1];
    for (int i = 1; i < count; i++) {
        if (pts[i][1] < min_y) min_y = pts[i][1];
        if (pts[i][1] > max_y) max_y = pts[i][1];
    }

    // 화면 경계 클리핑 (예시)
    if (min_y < 0) min_y = 0;
    if (max_y >= vmode_info.height) max_y = vmode_info.height - 1;

    // 각 스캔라인을 순회
    for (int y = min_y; y <= max_y; y++) {
        int nodes[64];  // 최대 꼭짓점 수에 따라 조절 가능
        int node_count = 0;

        // 모든 변에 대해 현재 y와의 교점 x를 계산
        for (int i = 0; i < count; i++) {
            int p1[2] = {
                pts[i][0],
                pts[i][1],
            };
            int p2[2] = {
                pts[(i + 1) % count][0],
                pts[(i + 1) % count][1],
            };

            // 수평선은 무시하고, y가 변의 범위 안에 있는지 확인
            if ((p1[1] < y && p2[1] >= y) || (p2[1] < y && p1[1] >= y)) {
                // 정수 연산만 사용 (나눗셈 1회 발생)
                // 선형 보간: x = x1 + (y - y1) * (x2 - x1) / (y2 - y1)
                int x = p1[0] + (y - p1[1]) * (p2[0] - p1[0]) / (p2[1] - p1[1]);
                nodes[node_count++] = x;
            }
        }

        // x 교점들을 오름차순으로 정렬 (단순 삽입 정렬)
        for (int i = 1; i < node_count; i++) {
            int j = i;
            while (j > 0 && nodes[j - 1] > nodes[j]) {
                int temp = nodes[j];
                nodes[j] = nodes[j - 1];
                nodes[j - 1] = temp;
                j--;
            }
        }

        // 두 개씩 짝지어 그 사이를 수평선으로 채움
        for (int i = 0; i < node_count; i += 2) {
            if (i + 1 >= node_count) break;
            int x_start = nodes[i];
            int x_end = nodes[i + 1];

            // 수평선 직접 그리기 (성능 최적화 지점)
            if (x_start < 0) x_start = 0;
            if (x_end >= vmode_info.width) x_end = vmode_info.width - 1;

            for (int x = x_start; x <= x_end; x++) {
                framebuffer[y * vmode_info.width + x] = color;
            }
        }
    }
}

static void draw_image(int xpos, int ypos, int width, int height, const uint32_t *img)
{
    for (int y = ypos; y < ypos + height; y++) {
        for (int x = xpos; x < xpos + width; x++) {
            framebuffer[y * vmode_info.width + x] = blend_color(
                img[(y - ypos) * width + x - xpos],
                framebuffer[y * vmode_info.width + x]
            );
        }
    }
}

inline static int get_glyph_bit(const uint8_t *glyph_data, int cwidth, int x, int y)
{
    return glyph_data[(y * cwidth + x) / 8] & (0x80 >> ((y * cwidth + x) % 8));
}

static void draw_char(int xpos, int ypos, uint32_t color, wchar_t ch)
{
    status_t status;
    int use_fallback;
    int cwidth, cheight;
    const uint8_t *glyph;
    uint8_t font_glyph[32];
    static const uint8_t fallback_glyph[16] = {
        0x00,
        0x00,
        0x00,
        0x7E,
        0x66,
        0x5A,
        0x5A,
        0x7A,
        0x76,
        0x76,
        0x7E,
        0x76,
        0x76,
        0x7E,
        0x00,
        0x00,
    };

    use_fallback = 0;

    status = VlFont_GetGlyphDimension(ch, &cwidth, &cheight);
    if (!CHECK_SUCCESS(status) || (cwidth != 8 && cwidth != 16) || cheight != 16) {
        use_fallback = 1;
    }

    if (!use_fallback) {
        status = VlFont_GetGlyphData(ch, font_glyph, sizeof(font_glyph));
        if (!CHECK_SUCCESS(status)) {
            use_fallback = 1;
        }
    }

    if (use_fallback) {
        cwidth = 8;
        cheight = 16;
    }

    glyph = use_fallback ? fallback_glyph : font_glyph;

    for (int y = 0; y < cheight; y++) {
        for (int x = 0; x < cwidth; x++) {
            int draw = get_glyph_bit(glyph, cwidth, x, y);

            if (x > 0) {
                draw = draw || get_glyph_bit(glyph, cwidth, x - 1, y);
            }

            if (draw) {
                framebuffer[(ypos + y) * vmode_info.width + xpos + x] = color;
            }
        }
    }
}

static void draw_text(int xpos, int ypos, uint32_t color, const wchar_t *str)
{
    while (*str) {
        int cwidth = wcwidth(*str) * 8;

        draw_char(xpos, ypos, color, *str);

        xpos += cwidth;
        str++;
    }
}

static const uint32_t close_icon[] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0x00000000,
    0x00000000, 0xFF000000, 0xFF000000, 0xFF000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xFF000000, 0xFF000000, 0xFF000000, 0x00000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000,
    0xFF000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0xFF000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFF000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFF000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000,
    0xFF000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0xFF000000, 0x00000000, 0x00000000,
    0x00000000, 0xFF000000, 0xFF000000, 0xFF000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xFF000000, 0xFF000000, 0xFF000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

static const uint32_t maximize_icon[] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0x00000000,
    0x00000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFF000000, 0xFF000000, 0xFF000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0x00000000,
    0x00000000, 0xFF000000, 0xFF000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0xFF000000, 0xFF000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0x00000000,
    0x00000000, 0xFF000000, 0xFF000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0xFF000000, 0xFF000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0x00000000,
    0x00000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFF000000, 0xFF000000, 0xFF000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

static const uint32_t minimize_icon[] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFF000000, 0xFF000000, 0xFF000000, 0x00000000, 0x00000000, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

static void draw_button_frame(int xpos, int ypos, int width, int height)
{
    draw_line(xpos, ypos, xpos + width - 2, ypos, color_palette[15]);
    draw_line(xpos, ypos, xpos, ypos + height - 2, color_palette[15]);
    draw_line(xpos + width - 2, ypos + 1, xpos + width - 2, ypos + height - 2, color_palette[8]);
    draw_line(xpos + 1, ypos + height - 2, xpos + width - 3, ypos + height - 2, color_palette[8]);
    draw_line(xpos + width - 1, ypos, xpos + width - 1, ypos + height - 1, color_palette[0]);
    draw_line(xpos, ypos + height - 1, xpos + width - 1, ypos + height - 1, color_palette[0]);
    draw_rect(xpos + 1, ypos + 1, width - 3, height - 3, color_palette[7], 1);
}

static void draw_window(int xpos, int ypos, int width, int height)
{
    draw_rect(xpos, ypos, width - 1, height - 1, color_palette[8], 0);
    draw_line(xpos + 1, ypos + 1, xpos + width - 2, ypos + 1, color_palette[15]);
    draw_line(xpos + 1, ypos + 2, xpos + 1, ypos + height - 3, color_palette[15]);
    draw_line(xpos, ypos + height - 1, xpos + width - 1, ypos + height - 1, color_palette[0]);
    draw_line(xpos + width - 1, ypos, xpos + width - 1, ypos + height - 2, color_palette[0]);
    draw_rect(xpos + 2, ypos + 2, width - 4, height - 4, color_palette[7], 1);

    draw_rect(xpos + 4, ypos + 4, width - 8, 20, color_palette[1], 1);
    draw_button_frame(xpos + 4, ypos + 4, 20, 20);
    draw_image(xpos + 8, ypos + 8, 12, 12, close_icon);
    draw_button_frame(xpos + 24, ypos + 4, 20, 20);
    draw_image(xpos + 28, ypos + 8, 12, 12, minimize_icon);
    draw_button_frame(xpos + 44, ypos + 4, 20, 20);
    draw_image(xpos + 48, ypos + 8, 12, 12, maximize_icon);
    draw_text(xpos + (width - 104) / 2, ypos + 6, color_palette[15], L"Hello, World!");
}

static void draw_frame(int xpos, int ypos, int width, int height)
{
    draw_line(xpos, ypos, xpos + width - 2, ypos, color_palette[8]);
    draw_line(xpos, ypos, xpos, ypos + height - 2, color_palette[8]);
    draw_line(xpos + width - 1, ypos, xpos + width - 1, ypos + height - 1, color_palette[15]);
    draw_line(xpos, ypos + height - 1, xpos + width - 1, ypos + height - 1, color_palette[15]);
    draw_rect(xpos + 1, ypos + 1, width - 2, height - 2, color_palette[7], 1);
}

static const uint32_t cursor_data[21][12] = {
    {
        0xFF000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    },
    {
        0xFF000000,
        0xFF000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    },
    {
        0xFF000000,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    },
    {
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    },
    {
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    },
    {
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    },
    {
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    },
    {
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    },
    {
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
        0x00000000,
        0x00000000,
    },
    {
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
        0x00000000,
    },
    {
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
    },
    {
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
    },
    {
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0xFF000000,
        0xFF000000,
        0xFF000000,
        0xFF000000,
    },
    {
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    },
    {
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
        0x00000000,
        0x00000000,
    },
    {
        0xFF000000,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
        0x00000000,
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
        0x00000000,
        0x00000000,
    },
    {
        0xFF000000,
        0xFF000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
        0x00000000,
    },
    {
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
        0x00000000,
    },
    {
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
    },
    {
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0xFF000000,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFF000000,
        0x00000000,
    },
    {
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0xFF000000,
        0xFF000000,
        0x00000000,
        0x00000000,
    },
};

static int has_drawn_before = 0;
static int prev_xpos, prev_ypos;
static uint32_t prev_cursor_area_data[21][12];

static status_t mouse_move_to(int xpos, int ypos)
{
    status_t status;

    if (has_drawn_before) {
        for (int y = prev_ypos; y < MIN(prev_ypos + 21, vmode_info.height); y++) {
            for (int x = prev_xpos; x < MIN(prev_xpos + 12, vmode_info.width); x++) {
                framebuffer[y * vmode_info.width + x] =
                    prev_cursor_area_data[y - prev_ypos][x - prev_xpos];
            }
        }

        status = fbif->invalidate(fbdev, prev_xpos, prev_ypos, prev_xpos + 12, prev_ypos + 21);
        if (!CHECK_SUCCESS(status)) return status;
    }

    prev_xpos = xpos;
    prev_ypos = ypos;
    for (int y = ypos; y < MIN(ypos + 21, vmode_info.height); y++) {
        for (int x = xpos; x < MIN(xpos + 12, vmode_info.width); x++) {
            uint32_t cursor_pixel = cursor_data[y - ypos][x - xpos];
            uint32_t fb_pixel = framebuffer[y * vmode_info.width + x];

            prev_cursor_area_data[y - ypos][x - xpos] = fb_pixel;

            framebuffer[y * vmode_info.width + x] = blend_color(cursor_pixel, fb_pixel);
        }
    }

    has_drawn_before = 1;

    status = fbif->invalidate(fbdev, xpos, ypos, xpos + 12, ypos + 21);
    if (!CHECK_SUCCESS(status)) return status;

    status = fbif->flush(fbdev);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static int guishell_handler(struct shell_instance *inst, int argc, char **argv)
{
    status_t status;
    uint16_t key, flags;
    int mouse_xpos = 0, mouse_ypos = 0, should_exit;

    status = VlDev_Find("video0", &fbdev);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: cannot find device\n", argv[0]);
        return 1;
    }

    status = fbdev->driver->get_interface(fbdev, "video", (const void **)&vidif);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: cannot get interface from device\n", argv[0]);
        return 1;
    }

    status = fbdev->driver->get_interface(fbdev, "framebuffer", (const void **)&fbif);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: cannot get interface from device\n", argv[0]);
        return 1;
    }

    status = VlDev_Find("kbd0", &kbdev);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: cannot find device\n", argv[0]);
        return 1;
    }

    status = kbdev->driver->get_interface(kbdev, "hid", (const void **)&kbhidif);
    if (!CHECK_SUCCESS(status)) {
        fprintf(stderr, "%s: cannot get interface from device\n", argv[0]);
        return 1;
    }

    status = VlDev_Find("mouse0", &msdev);
    if (!CHECK_SUCCESS(status)) {
        msdev = NULL;
    }

    if (msdev) {
        status = msdev->driver->get_interface(msdev, "hid", (const void **)&mshidif);
        if (!CHECK_SUCCESS(status)) {
            fprintf(stderr, "%s: cannot get interface from device\n", argv[0]);
            return 1;
        }
    }

    status = fbif->get_framebuffer(fbdev, (void **)&framebuffer);
    if (!CHECK_SUCCESS(status)) return 1;

    status = vidif->get_mode(fbdev, &current_vmode);
    if (!CHECK_SUCCESS(status)) return 1;

    status = vidif->get_mode_info(fbdev, current_vmode, &vmode_info);
    if (!CHECK_SUCCESS(status)) return 1;

    draw_rect(0, 0, vmode_info.width, vmode_info.height, color_palette[3], 1);

    draw_window(50, 50, vmode_info.width - 100, vmode_info.height - 100);

    draw_circle(150, 200, 50, color_palette[15], 1);
    draw_ellipse_rect(220, 200, 300, 400, color_palette[15], 1);
    draw_bezier2(150, 200, 500, 220, 300, 200, color_palette[15]);

    status = fbif->invalidate(fbdev, 0, 0, vmode_info.width - 1, vmode_info.height - 1);
    if (!CHECK_SUCCESS(status)) return 1;

    status = fbif->flush(fbdev);
    if (!CHECK_SUCCESS(status)) return 1;

    if (msdev) {
        mouse_xpos = vmode_info.width / 2;
        mouse_ypos = vmode_info.height / 2;
        mouse_move_to(mouse_xpos, mouse_ypos);
    }

    int points[9][2];
    int current_point_index = 0;

    should_exit = 0;
    while (!should_exit) {
        status = kbhidif->poll_event(kbdev, &key, &flags);
        if (CHECK_SUCCESS(status) && status != STATUS_NO_EVENT) {
            if (flags & KEY_FLAG_BREAK) continue;

            switch (key) {
            case KEY_ESC:
                should_exit = 1;
                break;
            default:
                break;
            }
        }

        if (!msdev) continue;

        status = mshidif->poll_event(msdev, &key, &flags);
        if (CHECK_SUCCESS(status) && status != STATUS_NO_EVENT) {
            switch (flags & KEY_FLAG_TYPEMASK) {
            case 0:
                if (flags & KEY_FLAG_BREAK) break;

                if (key == KEY_MOUSEBTNL) {
                    if (54 <= mouse_xpos && mouse_xpos < 74 && 54 <= mouse_ypos &&
                        mouse_ypos < 74) {
                        should_exit = 1;
                    } else {
                        points[current_point_index][0] = mouse_xpos;
                        points[current_point_index][1] = mouse_ypos;

                        if (current_point_index == 8) {
                            draw_polygon(points, 9, color_palette[0], 1);

                            fbif->invalidate(
                                fbdev,
                                0,
                                0,
                                vmode_info.width - 1,
                                vmode_info.height - 1
                            );
                            fbif->flush(fbdev);
                        }

                        current_point_index = (current_point_index + 1) % 9;
                    }
                }
                break;
            case KEY_FLAG_XMOVE:
                if ((flags & KEY_FLAG_NEGATIVE)) {
                    if (mouse_xpos < key) {
                        mouse_xpos = 0;
                    } else {
                        mouse_xpos -= key;
                    }
                } else if (!(flags & KEY_FLAG_NEGATIVE)) {
                    if (vmode_info.width <= mouse_xpos + key) {
                        mouse_xpos = vmode_info.width - 1;
                    } else {
                        mouse_xpos += key;
                    }
                }
                break;
            case KEY_FLAG_YMOVE:
                if ((flags & KEY_FLAG_NEGATIVE)) {
                    if (vmode_info.height <= mouse_ypos + key) {
                        mouse_ypos = vmode_info.height - 1;
                    } else {
                        mouse_ypos += key;
                    }
                } else if (!(flags & KEY_FLAG_NEGATIVE)) {
                    if (mouse_ypos < key) {
                        mouse_ypos = 0;
                    } else {
                        mouse_ypos -= key;
                    }
                }

                mouse_move_to(mouse_xpos, mouse_ypos);
                break;
            default:
                break;
            }
        }
    }

    VlShell_Execute(NULL, "clear");

    return 0;
}

static struct command guishell_command = {
    .name = "guishell",
    .handler = guishell_handler,
    .help_message = "Run GUI shell",
};

__constructor static void init()
{
    VlShell_RegisterCommand(&guishell_command);
}

status_t _start(int argc, char **argv)
{
    return STATUS_SUCCESS;
}

__destructor static void deinit(void)
{
    VlShell_UnregisterCommand(&guishell_command);
}
