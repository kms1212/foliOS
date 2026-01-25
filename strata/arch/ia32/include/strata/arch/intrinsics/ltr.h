#ifndef __STRATA_ARCH_INTRINSICS_LTR_H__
#define __STRATA_ARCH_INTRINSICS_LTR_H__

#include <stdint.h>

#include <strata/compiler.h>

__always_inline void StA_Ltr(uint16_t sel)
{
    __asm__ volatile("ltr %%ax" : : "a"(sel));
}

#endif  // __STRATA_ARCH_INTRINSICS_LTR_H__
