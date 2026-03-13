#ifndef __VELLUM_ARCH_IDT_H__
#define __VELLUM_ARCH_IDT_H__

#include <stdint.h>

#include <vellum/compiler.h>

struct VlA_Idtr {
    uint16_t size;
    uint32_t idt_ptr;
} __packed;

struct VlA_IdtEntry {
    uint16_t offset_low;
    uint16_t segment_selector;
    uint8_t reserved1;
    uint8_t attributes;
    uint16_t offset_high;
} __packed;

extern struct VlA_Idtr _ia32_idtr;

#endif  // __VELLUM_ARCH_IDT_H__
