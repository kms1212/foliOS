#ifndef __STRATA_ARCH_BREAKPOINT_H__
#define __STRATA_ARCH_BREAKPOINT_H__

#include <strata/compiler.h>

__always_inline void StA_Breakpoint(void)
{
    __asm__ volatile ("xchg %%bx, %%bx");
}

#endif // __STRATA_ARCH_BREAKPOINT_H__
