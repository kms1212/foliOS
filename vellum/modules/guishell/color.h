#ifndef __COLOR_H__
#define __COLOR_H__

#include <stdint.h>

#include <vellum/compiler.h>

#include "types.h"

typedef union {
    uint32_t raw;
    struct {
        uint32_t a : 8;
        uint32_t r : 8;
        uint32_t g : 8;
        uint32_t b : 8;
    } __packed;
} color_t;

enum brush_type {
    BRUSH_TYPE_NONE = 0,
    BRUSH_TYPE_SOLID,
    BRUSH_TYPE_GRADIENT,
    BRUSH_TYPE_PATTERN,
};

struct brush {
    enum brush_type type;

    color_t primary_color;
    color_t secondary_color;
};

struct pen {
    color_t primary_color;
    color_t secondary_color;
};

color_t color_blend(color_t upper, color_t lower);

color_t color_resolve_brush(
    const struct brush *b, struct point2 start, struct point2 end, struct point2 current
);
color_t color_resolve_pen(
    const struct pen *b, struct point2 start, struct point2 end, struct point2 current
);

#endif  // __COLOR_H__
