#ifndef __DRAW_H__
#define __DRAW_H__

#include <stdint.h>
#include <wchar.h>

#include "types.h"
#include "color.h"
#include "bitmap.h"

struct drawing_context {
    const struct brush *brush;
    const struct pen *pen;
    struct rect clipping_box;
    struct bitmap *target;
};

void draw_set_target(struct drawing_context *ctx, struct bitmap *target);
void draw_set_brush(struct drawing_context *ctx, const struct brush *brush);
void draw_set_pen(struct drawing_context *ctx, const struct pen *pen);
void draw_set_clipping_box(struct drawing_context *ctx, struct rect box);

void draw_pixel(struct drawing_context *ctx, struct point2 point, color_t color);
void draw_line(struct drawing_context *ctx, struct point2 p0, struct point2 p1);
void draw_rect(struct drawing_context *ctx, struct rect2 area, int fill);
void draw_circle(struct drawing_context *ctx, struct point2 center, int radius, int fill);
void draw_ellipse_rect(struct drawing_context *ctx, struct rect2 area, int fill);
void draw_bezier2(struct drawing_context *ctx, struct point2 points[3]);
void draw_bezier3(struct drawing_context *ctx, struct point2 points[4]);
void draw_bitmap(struct drawing_context *ctx, struct rect2 area, const struct bitmap *src);
void draw_char(struct drawing_context *ctx, struct point2 pos, wchar_t ch);
void draw_text(struct drawing_context *ctx, struct point2 pos, const wchar_t *str);

#endif // __DRAW_H__
