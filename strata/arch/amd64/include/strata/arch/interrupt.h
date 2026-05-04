#ifndef __STRATA_ARCH_INTERRUPT_H__
#define __STRATA_ARCH_INTERRUPT_H__

#include <stdint.h>

#include <strata/arch/intrinsics/misc.h>

#include <strata/compiler.h>
#include <strata/macros.h>

#define HAS_ERROR_BITMAP 0x60227D00
#define IS_FAULT_BITMAP  0x603B7FE1

struct StA_InterruptFrame {
    uint64_t error;
    uint64_t rip;
    uint16_t cs;
    RESERVE_2BYTES;
    RESERVE_2BYTES;
    RESERVE_2BYTES;
    uint64_t rflags;
    uint64_t rsp;
    uint16_t ss;
    RESERVE_2BYTES;
    RESERVE_2BYTES;
    RESERVE_2BYTES;
} __packed;

__always_inline void StA_EnableInterrupt(void)
{
    StA_Sti();
}
__always_inline void StA_DisableInterrupt(void)
{
    StA_Cli();
}

__always_inline uint32_t StA_SaveInterrupt(void)
{
    uint64_t flags;

    __asm__ volatile("pushfq\r\n"
                     "pop    %0\r\n"
                     : "=r"(flags));

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

#endif  // __STRATA_ARCH_INTERRUPT_H__
