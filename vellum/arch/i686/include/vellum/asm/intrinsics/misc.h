#ifndef __VELLUM_ASM_INTRINSICS_MISC_H__
#define __VELLUM_ASM_INTRINSICS_MISC_H__

#include <cpuid.h>

#include <stdint.h>

#include <vellum/compiler.h>

__always_inline void _i686_interrupt_enable(void)
{
    __asm__ volatile ("sti");
}

__always_inline void _i686_interrupt_disable(void)
{
    __asm__ volatile ("cli");
}

__always_inline void _i686_halt(void)
{
    __asm__ volatile ("hlt");
}

__always_inline void _i686_pause(void)
{
    __asm__ volatile ("pause");
}

#endif // __VELLUM_ASM_INTRINSICS_MISC_H__
