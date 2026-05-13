#ifndef __STRATA_ARCH_INTRINSICS_GDT_H__
#define __STRATA_ARCH_INTRINSICS_GDT_H__

#include <stdint.h>

#include <strata/arch/gdt.h>

#include <strata/compiler.h>

__always_inline void StA_Lgdt(struct StA_Gdtr *gdtr __in)
{
    __asm__ volatile("lgdt (%0)" : : "r"(gdtr));
}

#endif  // __STRATA_ARCH_INTRINSICS_GDT_H__
