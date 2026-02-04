#ifndef __STRATA_ARCH_INTRINSICS_XSAVE_H__
#define __STRATA_ARCH_INTRINSICS_XSAVE_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/macros.h>

struct StA_FXSaveSt {
    uint64_t st_low;
    uint16_t st_high;
    uint16_t : 16;
    uint16_t : 16;
    uint16_t : 16;
} __packed;

struct StA_FXSaveXmm {
    uint64_t xmm_low;
    uint64_t xmm_high;
} __packed;

struct StA_FXSaveLegacyBuffer {
    uint16_t fcw;
    uint16_t fsw;
    uint8_t ftw;
    RESERVE_1BYTE;
    uint16_t fop;
    uint32_t fip;
    uint16_t fcs;
    RESERVE_2BYTES;
    uint32_t fdp;
    uint16_t fds;
    RESERVE_2BYTES;
    uint32_t mxcsr;
    uint32_t mxcsr_mask;
    struct StA_FXSaveSt st[8];
    struct StA_FXSaveXmm xmm[8];
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
} __packed;

struct StA_FXSaveDefaultBuffer {
    uint16_t fcw;
    uint16_t fsw;
    uint8_t ftw;
    RESERVE_1BYTE;
    uint16_t fop;
    uint32_t fip;
    uint16_t fcs;
    RESERVE_2BYTES;
    uint32_t fdp;
    uint16_t fds;
    RESERVE_2BYTES;
    uint32_t mxcsr;
    uint32_t mxcsr_mask;
    struct StA_FXSaveSt st[8];
    struct StA_FXSaveXmm xmm[16];
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
} __packed;

struct StA_FXSavePromotedBuffer {
    uint16_t fcw;
    uint16_t fsw;
    uint8_t ftw;
    RESERVE_1BYTE;
    uint16_t fop;
    uint64_t fip;
    uint64_t fdp;
    uint32_t mxcsr;
    uint32_t mxcsr_mask;
    struct StA_FXSaveSt st[8];
    struct StA_FXSaveXmm xmm[16];
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
    RESERVE_8BYTES;
} __packed;

union StA_FXSaveBuffer {
    struct StA_FXSaveLegacyBuffer leg;
    struct StA_FXSaveDefaultBuffer def;
    struct StA_FXSavePromotedBuffer pro;
} __packed;

__always_inline void StA_FXSave(union StA_FXSaveBuffer *buf)
{
    __asm__ volatile("fxsave64 %0" : : "m"(*buf));
}

__always_inline void StA_FXRestore(union StA_FXSaveBuffer *buf)
{
    __asm__ volatile("fxrstor64 %0" : : "m"(*buf));
}

#endif  // __STRATA_ARCH_INTRINSICS_XSAVE_H__
