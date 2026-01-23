#ifndef __STRATA_ARCH_INTERRUPT_H__
#define __STRATA_ARCH_INTERRUPT_H__

#include <stdint.h>

#include <strata/arch/intrinsics/misc.h>

#include <strata/compiler.h>

struct StA_InterruptFrame {
    uint32_t error;
    uint32_t eip;
    uint16_t cs;
    uint16_t :16;
    uint32_t eflags;
    uint32_t user_esp;
    uint16_t user_ss;
    uint16_t :16;
} __packed;

#define StA_EnableInterrupt StA_Sti
#define StA_DisableInterrupt StA_Cli

__always_inline uint32_t StA_SaveInterrupt(void)
{
    uint32_t flags;

    __asm__ volatile (
        "pushfl\r\n"
        "pop    %0\r\n"
        : "=r"(flags)
    );

    return !!(flags & 0x0200);
}

__always_inline void StA_RestoreInterrupt(uint32_t state)
{
    if (state) {
        StA_EnableInterrupt();
    } else {
        StA_DisableInterrupt();
    }
}

#endif // __STRATA_ARCH_INTERRUPT_H__
