#ifndef __VELLUM_ASM_INTRINSICS_INVLPG_H__
#define __VELLUM_ASM_INTRINSICS_INVLPG_H__

#include <vellum/compiler.h>

__always_inline void _i686_invlpg(void *addr)
{
    __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

#endif // __VELLUM_ASM_INTRINSICS_INVLPG_H__
