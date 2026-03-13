#ifndef __VELLUM_ARCH_INTERRUPT_H__
#define __VELLUM_ARCH_INTERRUPT_H__

#include <stdint.h>

#include <vellum/arch/intrinsics/misc.h>

#include <vellum/compiler.h>

struct VlA_InterruptFrame {
    uint32_t error;
    uint32_t eip;
    uint16_t cs;
    uint16_t reserved1;
    uint32_t eflags;
};

#define VlA_EnableInterrupt  VlA_Sti
#define VlA_DisableInterrupt VlA_Cli

__always_inline uint32_t VlA_SaveInterrupt(void)
{
    uint32_t flags;

    __asm__ volatile("pushfd\r\n"
                     "pop    %0\r\n"
                     : "=r"(flags));

    return !!(flags & 0x0200);
}

__always_inline void VlA_RestoreInterrupt(uint32_t state)
{
    if (state) {
        VlA_EnableInterrupt();
    } else {
        VlA_DisableInterrupt();
    }
}

#endif  // __VELLUM_ARCH_INTERRUPT_H__
