#ifndef __VELLUM_ARCH_IO_H__
#define __VELLUM_ARCH_IO_H__

#include <stdint.h>

#include <vellum/arch/intrinsics/io.h>

#include <vellum/compiler.h>

__always_inline void StIoA_Wait(void)
{
    __asm__ volatile("outb %%al, $0x80" : : "a"(0));
}

#endif  // __VELLUM_ARCH_IO_H__
