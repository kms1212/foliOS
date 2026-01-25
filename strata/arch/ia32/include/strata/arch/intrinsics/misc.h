#ifndef __STRATA_ARCH_INTRINSICS_MISC_H__
#define __STRATA_ARCH_INTRINSICS_MISC_H__

#include <cpuid.h>
#include <stdint.h>

#include <strata/compiler.h>

__always_inline void StA_Sti(void)
{
    __asm__ volatile("sti");
}

__always_inline void StA_Cli(void)
{
    __asm__ volatile("cli");
}

__always_inline void StA_Halt(void)
{
    __asm__ volatile("hlt");
}

__always_inline void StA_Pause(void)
{
    __asm__ volatile("pause");
}

#endif  // __STRATA_ARCH_INTRINSICS_MISC_H__
