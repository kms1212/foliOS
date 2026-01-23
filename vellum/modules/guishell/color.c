#include "color.h"

color_t color_blend(color_t upper, color_t lower)
{
    color_t new_color;

    int alpha = upper.a;

    if (!alpha) {
        new_color.raw = lower.raw;
    } else if (alpha == 0xFF) {
        new_color.raw = upper.raw;
    } else {
        new_color.r = (int)lower.r * (255 - alpha) / 255 + (int)upper.r * alpha / 255;
        new_color.g = (int)lower.g * (255 - alpha) / 255 + (int)upper.g * alpha / 255;
        new_color.b = (int)lower.b * (255 - alpha) / 255 + (int)upper.b * alpha / 255;
    }
    new_color.a = 0;

    return new_color;
}

static color_t resolve_gradient(color_t pri, color_t sec, int numer, int denom)
{
    color_t new_color;
    int alpha = numer * 255 / denom;

    new_color.a = (int)pri.a * alpha / 255 + (int)sec.a * (255 - alpha) / 255;
    new_color.r = (int)pri.r * alpha / 255 + (int)sec.r * (255 - alpha) / 255;
    new_color.g = (int)pri.g * alpha / 255 + (int)sec.g * (255 - alpha) / 255;
    new_color.b = (int)pri.b * alpha / 255 + (int)sec.b * (255 - alpha) / 255;

    return new_color;
}

color_t color_resolve_brush(const struct brush *b, struct point2 start, struct point2 end, struct point2 current)
{
    color_t new_color;
    int numer, denom;

    switch (b->type) {
        case BRUSH_TYPE_NONE:
            new_color.a = 0xFF;
            new_color.r = 0;
            new_color.g = 0;
            new_color.b = 0;
            break;
        case BRUSH_TYPE_SOLID:
            new_color.raw = b->primary_color.raw;
            break;
        case BRUSH_TYPE_GRADIENT:
            numer = current.x - start.x;
            denom = end.x - start.x;
            if ((numer > 0 && denom < 0) || (numer < 0 && denom > 0)) {
                numer = 0;
            } else if ((numer < 0 && numer < denom) || (numer > 0 && numer > denom)) {
                numer = denom;
            }
            new_color.raw = resolve_gradient(b->primary_color, b->secondary_color, numer, denom).raw;
            break;
        case BRUSH_TYPE_PATTERN:
            new_color.raw = ((current.x + current.y) & 1) ? b->primary_color.raw : b->secondary_color.raw;
            break;
        default:
            break;
    }

    return new_color;
}
