#ifndef __VELLUM_RESOURCE_H__
#define __VELLUM_RESOURCE_H__

#include <stdint.h>

enum resource_type {
    RT_IOPORT = 0,
    RT_MEMORY,
    RT_IRQ,
    RT_DMA,
    RT_BUS,
    RT_LBA,
};

struct resource {
    struct resource *next;

    enum resource_type type;
    uint64_t base;
    uint64_t limit;
    uint32_t flags;
};

struct resource *VlRes_Create(struct resource *prev);

#endif  // __VELLUM_RESOURCE_H__
