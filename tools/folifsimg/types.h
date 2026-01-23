#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>

typedef int64_t lba_t;

typedef struct {
    uint8_t bytes[16];
} __attribute__((packed)) uuid_t;

#endif // __TYPES_H__
