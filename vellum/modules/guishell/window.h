#ifndef __WINDOW_H__
#define __WINDOW_H__

#include <stdint.h>

#include <emos/status.h>

#include "types.h"


struct window {
    struct window *next;

    struct rect2 area;
    char *title;
    uint32_t *framebuffer;

    int is_active;
    int is_dragging;
};

status_t window_create(struct window *window);

#endif // __WINDOW_H__
