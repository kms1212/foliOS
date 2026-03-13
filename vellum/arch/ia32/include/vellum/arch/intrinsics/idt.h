#ifndef __VELLUM_ARCH_INTRINSICS_IDT_H__
#define __VELLUM_ARCH_INTRINSICS_IDT_H__

#include <stdint.h>

#include <vellum/arch/idt.h>

#include <vellum/compiler.h>

__always_inline void VlA_Lidt(struct VlA_Idtr *idtr)
{
    __asm__ volatile("lidt (%0)" : : "r"(idtr));
}

#endif  // __VELLUM_ARCH_INTRINSICS_IDT_H__
