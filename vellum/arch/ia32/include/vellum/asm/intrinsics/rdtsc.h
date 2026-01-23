#ifndef __VELLUM_ASM_INTRINSICS_RDTSC_H__
#define __VELLUM_ASM_INTRINSICS_RDTSC_H__

#include <stdint.h>

#include <vellum/compiler.h>

__always_inline uint64_t _ia32_rdtsc(void)
{
    uint32_t low, high;
    __asm__ volatile ("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}


#endif // __VELLUM_ASM_INTRINSICS_RDTSC_H__
