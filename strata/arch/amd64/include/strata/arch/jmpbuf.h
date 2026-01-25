#ifndef __STRATA_ARCH_JMPBUF_H__
#define __STRATA_ARCH_JMPBUF_H__

#include <stdint.h>

struct StA_JumpBuffer {
    uint32_t addr;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t eax;
};

#endif  // __STRATA_ARCH_JMPBUF_H__
