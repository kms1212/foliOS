#ifndef __STRATA_ARCH_TSS_H__
#define __STRATA_ARCH_TSS_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/macros.h>

struct StA_Tss {
    RESERVE_4BYTES;
    uint32_t rsp0_low;
    uint32_t rsp0_high;
    uint32_t rsp1_low;
    uint32_t rsp1_high;
    uint32_t rsp2_low;
    uint32_t rsp2_high;
    RESERVE_4BYTES;
    RESERVE_4BYTES;
    uint32_t ist1_low;
    uint32_t ist1_high;
    uint32_t isr2_low;
    uint32_t isr2_high;
    uint32_t isr3_low;
    uint32_t isr3_high;
    uint32_t isr4_low;
    uint32_t isr4_high;
    uint32_t isr5_low;
    uint32_t isr5_high;
    uint32_t isr6_low;
    uint32_t isr6_high;
    uint32_t isr7_low;
    uint32_t isr7_high;
    RESERVE_4BYTES;
    RESERVE_4BYTES;
    RESERVE_2BYTES;
    uint16_t iomap_base;
} __packed __aligned(16);

#endif  // __STRATA_ARCH_TSS_H__
