#ifndef __STRATA_ARCH_INTRINSICS_IDT_H__
#define __STRATA_ARCH_INTRINSICS_IDT_H__

#include <stdint.h>

#include <strata/arch/idt.h>

#include <strata/compiler.h>

__always_inline void StA_Lidt(struct StA_Idtr *idtr __in)
{
    __asm__ volatile("lidt (%0)" : : "r"(idtr));
}

#endif  // __STRATA_ARCH_INTRINSICS_IDT_H__
