#ifndef __STRATA_ARCH_REGISTERS_H__
#define __STRATA_ARCH_REGISTERS_H__

#include <stdint.h>

struct StA_PushalResult {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
} __packed;

#endif  // __STRATA_ARCH_REGISTERS_H__
