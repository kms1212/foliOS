#ifndef __VELLUM_ASM_INTRINSICS_GDT_H__
#define __VELLUM_ASM_INTRINSICS_GDT_H__

#include <stdint.h>

#include <vellum/asm/gdt.h>

#include <vellum/compiler.h>

__always_inline void _ia32_lgdt(struct gdtr *gdtr)
{
    __asm__ volatile ("lgdt (%0)" : : "r"(gdtr));
}


#endif // __VELLUM_ASM_INTRINSICS_GDT_H__
