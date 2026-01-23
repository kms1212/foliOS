#ifndef __BITMAP_H__
#define __BITMAP_H__

#include <stdint.h>

#include "types.h"
#include "color.h"

struct bitmap {
    struct size2 size;
    color_t data[];
};

#endif // __BITMAP_H__
