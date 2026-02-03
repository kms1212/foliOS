#ifndef __STRATA_ARCH_INTRINSICS_MSR_H__
#define __STRATA_ARCH_INTRINSICS_MSR_H__

#include <stdint.h>

#include <strata/compiler.h>

#define MSR_EFER   0xC0000080
#define EFER_SCE   (1 << 0)
#define EFER_LME   (1 << 8)
#define EFER_LMA   (1 << 10)
#define EFER_NXE   (1 << 11)
#define EFER_SVME  (1 << 12)
#define EFER_LMSLE (1 << 13)
#define EFER_FFXSR (1 << 14)
#define EFER_TCE   (1 << 15)

#define MSR_STAR   0xC0000081
#define MSR_LSTAR  0xC0000082
#define MSR_SFMASK 0xC0000084

#define MSR_FS_BASE        0xC0000100
#define MSR_GS_BASE        0xC0000101
#define MSR_KERNEL_GS_BASE 0xC0000102

__always_inline uint64_t StA_ReadMsr(uint32_t msr)
{
    uint32_t high, low;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

__always_inline void StA_WriteMsr(uint32_t msr, uint64_t value)
{
    uint32_t high = value >> 32;
    uint32_t low = value & 0xFFFFFFFF;
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

__always_inline uint64_t StA_ReadTsc(void)
{
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

#endif  // __STRATA_ARCH_INTRINSICS_MSR_H__
