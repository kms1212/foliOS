#ifndef __VELLUM_ASM_INTERRUPT_H__
#define __VELLUM_ASM_INTERRUPT_H__

#include <stdint.h>

#include <vellum/asm/intrinsics/misc.h>

#include <vellum/compiler.h>

struct interrupt_frame {
    uint32_t error;
    uint32_t eip;
    uint16_t cs;
    uint16_t reserved1;
    uint32_t eflags;
};

#define interrupt_disable   _i686_interrupt_disable
#define interrupt_enable    _i686_interrupt_enable

#endif // __VELLUM_ASM_INTERRUPT_H__
