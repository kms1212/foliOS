#ifndef __VELLUM_ASM_INTRINSICS_IDT_H__
#define __VELLUM_ASM_INTRINSICS_IDT_H__

#include <stdint.h>

#include <vellum/asm/idt.h>

#include <vellum/compiler.h>

__always_inline void _ia32_lidt(struct idtr *idtr)
{
    __asm__ volatile ("lidt (%0)" : : "r"(idtr));
}


#endif // __VELLUM_ASM_INTRINSICS_IDT_H__
