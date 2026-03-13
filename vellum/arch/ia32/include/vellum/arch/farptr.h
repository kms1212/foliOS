#ifndef __VELLUM_ARCH_FARPTR_H__
#define __VELLUM_ARCH_FARPTR_H__

#include <stdint.h>

#include <vellum/compiler.h>

#define FARPTR16_TO_VPTR(far_ptr) ((void *)(((uintptr_t)(far_ptr).segment << 4) + (far_ptr).offset))

struct VlA_FarPtr16 {
    uint16_t offset;
    uint16_t segment;
} __packed;

struct VlA_FarPtr32 {
    uint32_t offset;
    uint16_t segment;
} __packed;

#endif  // __VELLUM_ARCH_FARPTR_H__
