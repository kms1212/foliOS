#ifndef __STRATA_MACROS_H__
#define __STRATA_MACROS_H__

#include <stdint.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define TEFLAG(val, mask) (((val) & (mask)) == (mask))

#define ALIGN(v, a)     (((v) + (a) - 1) & ~((a) - 1))
#define ALIGN_DIV(v, a) (((v) + (a) - 1) / (a))

#define RESERVE_1BYTE                                                                              \
uint8_t:                                                                                           \
    8
#define RESERVE_2BYTES                                                                             \
uint16_t:                                                                                          \
    16
#define RESERVE_4BYTES                                                                             \
uint32_t:                                                                                          \
    32
#define RESERVE_8BYTES                                                                             \
uint64_t:                                                                                          \
    64

#endif  // __STRATA_MACROS_H__
