#ifndef __STRATA_ARCH_IDT_H__
#define __STRATA_ARCH_IDT_H__

#include <stdint.h>

#include <strata/compiler.h>

struct StA_Idtr {
    uint16_t size;
    uint64_t idt_ptr;
} __packed;

struct StA_IdtEntry {
    uint16_t offset_low;
    uint16_t segment_selector;

    union {
        uint16_t raw;

        struct {
            uint16_t ist : 3;
            uint16_t : 5;
            uint16_t gate_type : 4;
            uint16_t : 1;
            uint16_t dpl : 2;
            uint16_t p : 1;
        } __packed;
    } __packed attributes;

    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t : 32;
} __packed;

#endif  // __STRATA_ARCH_IDT_H__
