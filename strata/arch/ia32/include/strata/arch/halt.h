#ifndef __STRATA_ARCH_HALT_H__
#define __STRATA_ARCH_HALT_H__

#include <strata/compiler.h>

__always_inline void StA_Hlt(void)
{
    __asm__ volatile("hlt");
}

#endif  // __STRATA_ARCH_HALT_H__
