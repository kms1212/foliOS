#ifndef __VELLUM_ASM_BIOS_BIOSCALL_H__
#define __VELLUM_ASM_BIOS_BIOSCALL_H__

#include <stdint.h>

union data_reg {
    uint32_t l;
    uint16_t w;
    struct {
        uint8_t l, h;
    } b;
};

union index_reg {
    uint32_t l;
    uint16_t w;
};

union pointer_reg {
    uint32_t l;
    uint16_t w;
};

union segment_reg {
    uint16_t w;
};

struct bioscall_regs {
    union data_reg a;
    union data_reg b;
    union data_reg c;
    union data_reg d;
    union index_reg si;
    union index_reg di;
    union pointer_reg bp;
    union segment_reg ds;
    union segment_reg es;
};

extern int VlBiosP_Call(uint8_t irq, struct bioscall_regs *);

#endif  // __VELLUM_ASM_BIOS_BIOSCALL_H__
