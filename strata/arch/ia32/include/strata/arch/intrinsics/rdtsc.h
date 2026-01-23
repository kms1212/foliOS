#ifndef __STRATA_ARCH_INTRINSICS_RDTSC_H__
#define __STRATA_ARCH_INTRINSICS_RDTSC_H__

#include <stdint.h>

#include <strata/compiler.h>

__always_inline uint64_t StA_Rdtsc(void)
{
    uint32_t low, high;
    __asm__ volatile ("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}


#endif // __STRATA_ARCH_INTRINSICS_RDTSC_H__
