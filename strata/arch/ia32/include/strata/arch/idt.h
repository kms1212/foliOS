#ifndef __STRATA_ARCH_IDT_H__
#define __STRATA_ARCH_IDT_H__

#include <stdint.h>

#include <strata/compiler.h>

struct StA_Idtr {
    uint16_t size;
    uint32_t idt_ptr;
} __packed;

struct StA_IdtEntry {
    uint16_t offset_low;
    uint16_t segment_selector;
    uint8_t reserved;

    union {
        uint8_t raw;

        struct {
            uint8_t gate_type : 4;
            uint8_t : 1;
            uint8_t dpl : 2;
            uint8_t p : 1;
        } __packed;
    } __packed attributes;

    uint16_t offset_high;
} __packed;

#endif  // __STRATA_ARCH_IDT_H__
