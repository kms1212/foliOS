#ifndef __STRATA_ARCH_PAUSE_H__
#define __STRATA_ARCH_PAUSE_H__

#include <strata/compiler.h>

__always_inline void StA_Pause(void)
{
    __asm__ volatile("pause");
}

#endif  // __STRATA_ARCH_PAUSE_H__
