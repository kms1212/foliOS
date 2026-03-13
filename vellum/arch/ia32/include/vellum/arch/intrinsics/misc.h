#ifndef __VELLUM_ARCH_INTRINSICS_MISC_H__
#define __VELLUM_ARCH_INTRINSICS_MISC_H__

#include <stdint.h>

#include <vellum/compiler.h>

__always_inline void VlA_Sti(void)
{
    __asm__ volatile("sti");
}

__always_inline void VlA_Cli(void)
{
    __asm__ volatile("cli");
}

__always_inline void VlA_Hlt(void)
{
    __asm__ volatile("hlt");
}

__always_inline void VlA_Pause(void)
{
    __asm__ volatile("pause");
}

#endif  // __VELLUM_ARCH_INTRINSICS_MISC_H__
