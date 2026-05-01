#ifndef __STRATA_ARCH_GDT_H__
#define __STRATA_ARCH_GDT_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/macros.h>

struct StA_Gdtr {
    uint16_t size;
    uint64_t gdt_ptr;
} __packed;

#define GDTSD_ACC_A         (1 << 0)
#define GDTSD_ACC_RW        (1 << 1)
#define GDTSD_ACC_DC        (1 << 2)
#define GDTSD_ACC_E         (1 << 3)
#define GDTSD_ACC_S         (1 << 4)
#define GDTSD_ACC_DPL_MASK  (3 << 5)
#define GDTSD_ACC_DPL_SHIFT 5
#define GDTSD_ACC_P         (1 << 7)

#define GDTSD_LF_LIMHI_MASK  (15 << 0)
#define GDTSD_LF_LIMHI_SHIFT 0
#define GDTSD_LF_AVL         (1 << 4)
#define GDTSD_LF_L           (1 << 5)
#define GDTSD_LF_DB          (1 << 6)
#define GDTSD_LF_G           (1 << 7)

struct StA_GdtSegmentDescriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access_byte;
    uint8_t limit_flags;
    uint8_t base_high;
} __packed;

#define GDTSSD_ACC_TYPE_MASK  (15 << 0)
#define GDTSSD_ACC_TYPE_SHIFT 0
#define GDTSSD_ACC_S          GDTSD_ACC_S
#define GDTSSD_ACC_DPL_MASK   GDTSD_ACC_DPL_MASK
#define GDTSSD_ACC_DPL_SHIFT  GDTSD_ACC_DPL_SHIFT
#define GDTSSD_ACC_P          GDTSD_ACC_P

#define GDTSSD_LF_LIMHI_MASK  GDTSD_LF_LIMHI_MASK
#define GDTSSD_LF_LIMHI_SHIFT GDTSD_LF_LIMHI_SHIFT
#define GDTSSD_LF_AVL         GDTSD_LF_AVL
#define GDTSSD_LF_L           GDTSD_LF_L
#define GDTSSD_LF_DB          GDTSD_LF_DB
#define GDTSSD_LF_G           GDTSD_LF_G

struct StA_GdtSystemSegmentDescriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid_low;
    uint8_t access_byte;
    uint8_t limit_flags;
    uint8_t base_mid_high;
    uint32_t base_high;
    RESERVE_4BYTES;
} __packed;

#endif  // __STRATA_ARCH_GDT_H__
