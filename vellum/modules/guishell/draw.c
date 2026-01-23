#include "draw.h"

#include <stdlib.h>

void draw_pixel(struct bitmap *target, struct point2 point, color_t color)
{
    if (point.x < 0 || point.y < 0 || point.x >= target->size.width || point.y >= target->size.height) {
        return;
    }

    target->data[point.y * target->size.width + point.x] = color;
}

static color_t get_pixel(const struct bitmap *src, struct point2 point)
{
    if (point.x < 0 || point.y < 0 || point.x >= src->size.width || point.y >= src->size.height) {
        return (color_t){ 0 };
    }

    return src->data[point.y * src->size.width + point.x];
}

void draw_line(struct bitmap *target, const struct brush *b, struct point2 p0, struct point2 p1)
{
    if (p0.x == p1.x) {
        for (int y = p0.y; y <= p1.y; y++) {
            color_t color = color_resolve_brush(b, p0, p1, POINT2(p0.x, y));
            draw_pixel(target, POINT2(p0.x, y), color);
        }
    } else if (p0.y == p1.y) {
        for (int x = p0.x; x <= p1.x; x++) {
            color_t color = color_resolve_brush(b, p0, p1, POINT2(x, p0.y));
            draw_pixel(target, POINT2(x, p0.y), color);
        }
    } else {
        int x = p0.x, y = p0.y;
        int dx =  abs (p1.x - p0.x), sx = p0.x < p1.x ? 1 : -1;
        int dy = -abs (p1.y - p0.y), sy = p0.y < p1.y ? 1 : -1; 
        int err = dx + dy, e2;
        color_t color;
       
        while (x != p1.x || y != p1.y) {
            color_resolve_brush(b, p0, p1, POINT2(x, y));
            draw_pixel(target, POINT2(x, y), color);

            e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y += sy;
            }
        }

        color_resolve_brush(b, p0, p1, POINT2(x, y));
    }
}

void draw_rect(struct bitmap *target, const struct brush *b, struct rect2 area, int fill)
{
    if (area.x + area.width > target->size.width) {
        area.width = target->size.width - area.x;
    }
    if (area.y + area.height > target->size.height) {
        area.height = target->size.height - area.y;
    }

    for (int x = area.x; x < area.x + area.width; x++) {
        draw_pixel(target, POINT2(x, area.y), b->primary_color);
    }

    for (int y = area.y + 1; y < area.y + area.height - 1; y++) {
        if (fill) {
            for (int x = area.x; x < area.x + area.width; x++) {
                draw_pixel(target, POINT2(x, y), b->primary_color);
            }
        } else {
            draw_pixel(target, POINT2(area.x, y), b->primary_color);
            draw_pixel(target, POINT2(area.x + area.width - 1, y), b->primary_color);
        }
    }

    for (int x = area.x; x < area.x + area.width; x++) {
        draw_pixel(target, POINT2(x, area.y + area.height - 1), b->primary_color);
    }
}

void draw_bitmap(struct bitmap *target, struct rect2 area, const struct bitmap *src)
{
    if (area.width > src->size.width) {
        area.width = src->size.width;
    }
    if (area.height > src->size.height) {
        area.height = src->size.height;
    }

    if (area.x + area.width > target->size.width) {
        area.width = target->size.width - area.x;
    }
    if (area.y + area.height > target->size.height) {
        area.height = target->size.height - area.y;
    }

    for (int y = area.y; y < area.y + area.height; y++) {
        for (int x = area.x; x < area.x + area.width; x++) {
            draw_pixel(target, POINT2(x, y), get_pixel(src, POINT2(x, y)));
        }
    }
}

