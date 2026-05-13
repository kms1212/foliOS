#ifndef __STRATA_ARCH_INTRINSICS_FPU_SIMD_H__
#define __STRATA_ARCH_INTRINSICS_FPU_SIMD_H__

#include <assert.h>
#include <stdint.h>

#include <strata/compiler.h>
#include <strata/macros.h>

struct StA_FXSaveSt {
    uint64_t st_low;
    uint16_t st_high;
    RESERVE_2BYTES;
    RESERVE_2BYTES;
    RESERVE_2BYTES;
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
    uint8_t raw[512];
} __aligned(16);

struct StA_XSaveBuffer {
    uint8_t raw[1024];
} __aligned(64);

union StA_XStateBuffer {
    union StA_FXSaveBuffer fx;
    struct StA_XSaveBuffer xs;
} __aligned(64);

__always_inline void StA_FXSave(union StA_FXSaveBuffer *buf __out)
{
    assert(buf);

    __asm__ volatile("fxsave64 %0" : : "m"(*buf));
}

__always_inline void StA_FXRestore(union StA_FXSaveBuffer *buf __in)
{
    __asm__ volatile("fxrstor64 %0" : : "m"(*buf));
}

__always_inline void StA_XSave(union StA_XStateBuffer *buf __out, uint64_t mask __in)
{
    assert(buf);

    uint32_t eax = (uint32_t)mask;
    uint32_t edx = (uint32_t)(mask >> 32);

    __asm__ volatile("xsave64 %0" : "=m"(*buf) : "a"(eax), "d"(edx) : "memory");
}

__always_inline void StA_XRestore(const union StA_XStateBuffer *buf __in, uint64_t mask __in)
{
    uint32_t eax = (uint32_t)mask;
    uint32_t edx = (uint32_t)(mask >> 32);

    __asm__ volatile("xrstor64 %0" : : "m"(*buf), "a"(eax), "d"(edx) : "memory");
}

__always_inline void StA_LdMxcsr(uint32_t value __in)
{
    __asm__ volatile("ldmxcsr %0" : : "m"(value));
}

__always_inline void StA_FNInit(void)
{
    __asm__ volatile("fninit");
}

__always_inline void StA_ZeroXmmRegisters(void)
{
    __asm__ volatile(
        "pxor %%xmm0, %%xmm0\n\t"
        "pxor %%xmm1, %%xmm1\n\t"
        "pxor %%xmm2, %%xmm2\n\t"
        "pxor %%xmm3, %%xmm3\n\t"
        "pxor %%xmm4, %%xmm4\n\t"
        "pxor %%xmm5, %%xmm5\n\t"
        "pxor %%xmm6, %%xmm6\n\t"
        "pxor %%xmm7, %%xmm7\n\t"
        "pxor %%xmm8, %%xmm8\n\t"
        "pxor %%xmm9, %%xmm9\n\t"
        "pxor %%xmm10, %%xmm10\n\t"
        "pxor %%xmm11, %%xmm11\n\t"
        "pxor %%xmm12, %%xmm12\n\t"
        "pxor %%xmm13, %%xmm13\n\t"
        "pxor %%xmm14, %%xmm14\n\t"
        "pxor %%xmm15, %%xmm15\n\t"
        :
        :
        : "memory"
    );
}

__always_inline void StA_VZeroAll(void)
{
    __asm__ volatile("vzeroall");
}

#endif  // __STRATA_ARCH_INTRINSICS_FPU_SIMD_H__
