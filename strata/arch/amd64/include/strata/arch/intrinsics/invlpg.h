#ifndef __STRATA_ARCH_INTRINSICS_INVLPG_H__
#define __STRATA_ARCH_INTRINSICS_INVLPG_H__

#include <strata/compiler.h>

__always_inline void StA_Invlpg(void *addr __in)
{
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

#endif  // __STRATA_ARCH_INTRINSICS_INVLPG_H__
