#ifndef __STRATA_ARCH_IO_H__
#define __STRATA_ARCH_IO_H__

#include <stdint.h>

#include <strata/arch/intrinsics/io.h>

#include <strata/compiler.h>

__always_inline void StIoA_Wait(void)
{
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

#endif // __STRATA_ARCH_IO_H__
