#ifndef __VELLUM_ARCH_INTRINSICS_GDT_H__
#define __VELLUM_ARCH_INTRINSICS_GDT_H__

#include <stdint.h>

#include <vellum/arch/gdt.h>

#include <vellum/compiler.h>

__always_inline void VlA_Lgdt(struct StA_Gdtr *gdtr)
{
    __asm__ volatile("lgdt (%0)" : : "r"(gdtr));
}

#endif  // __VELLUM_ARCH_INTRINSICS_GDT_H__
