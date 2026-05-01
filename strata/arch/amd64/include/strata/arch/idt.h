#ifndef __STRATA_ARCH_IDT_H__
#define __STRATA_ARCH_IDT_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/macros.h>

struct StA_Idtr {
    uint16_t size;
    uint64_t idt_ptr;
} __packed;

#define IDTGD_ATTR_IST_MASK       (7 << 0)
#define IDTGD_ATTR_IST_SHIFT      0
#define IDTGD_ATTR_GATETYPE_MASK  (15 << 8)
#define IDTGD_ATTR_GATETYPE_SHIFT 8
#define GDTSD_ACC_DPL_MASK        (3 << 13)
#define GDTSD_ACC_DPL_SHIFT       12
#define GDTSD_ACC_P               (1 << 15)

struct StA_IdtGateDescriptor {
    uint16_t offset_low;
    uint16_t segment_selector;
    uint16_t attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    RESERVE_4BYTES;
} __packed;

#endif  // __STRATA_ARCH_IDT_H__
