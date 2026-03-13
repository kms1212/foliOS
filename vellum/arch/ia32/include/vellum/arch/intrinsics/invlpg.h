#ifndef __VELLUM_ARCH_INTRINSICS_INVLPG_H__
#define __VELLUM_ARCH_INTRINSICS_INVLPG_H__

#include <vellum/compiler.h>

__always_inline void VlA_Invlpg(void *addr)
{
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

#endif  // __VELLUM_ARCH_INTRINSICS_INVLPG_H__
