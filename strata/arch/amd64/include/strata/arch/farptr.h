#ifndef __STRATA_ARCH_FARPTR_H__
#define __STRATA_ARCH_FARPTR_H__

#include <stdint.h>

#include <strata/compiler.h>

#define FARPTR16_TO_VPTR(far_ptr) ((void *)(((uintptr_t)(far_ptr).segment << 4) + (far_ptr).offset))

struct StA_FarPtr16 {
    uint16_t offset;
    uint16_t segment;
} __packed;

struct StA_FarPtr32 {
    uint32_t offset;
    uint16_t segment;
} __packed;

struct StA_FarPtr64 {
    uint64_t offset;
    uint16_t segment;
} __packed;

#endif  // __STRATA_ARCH_FARPTR_H__
