#ifndef __VELLUM_MACROS_H__
#define __VELLUM_MACROS_H__

#include <stdint.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define TEST_FLAG(val, mask) (((val) & (mask)) == (mask))

#define ALIGN(v, a)     (((v) + (a) - 1) & ~((a) - 1))
#define ALIGN_DIV(v, a) (((v) + (a) - 1) / (a))

#endif  // __VELLUM_MACROS_H__
