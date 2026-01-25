#ifndef __STRATA_ARCH_GDT_H__
#define __STRATA_ARCH_GDT_H__

#include <stdint.h>

#include <strata/compiler.h>

struct StA_Gdtr {
    uint16_t size;
    uint64_t gdt_ptr;
} __packed;

struct StA_GdtEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;

    union {
        uint8_t raw;

        struct {
            uint8_t a : 1;
            uint8_t rw : 1;
            uint8_t dc : 1;
            uint8_t e : 1;
            uint8_t s : 1;
            uint8_t dpl : 2;
            uint8_t p : 1;
        } __packed;
    } __packed access_byte;

    union {
        uint8_t raw;

        struct {
            uint8_t limit_high : 4;
            uint8_t avl : 1;
            uint8_t l : 1;
            uint8_t db : 1;
            uint8_t g : 1;
        } __packed;
    } __packed limit_flags;

    uint8_t base_high;
} __packed;

struct StA_GdtSystemSegmentEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid_low;

    union {
        uint8_t raw;

        struct {
            uint8_t type : 4;
            uint8_t s : 1;
            uint8_t dpl : 2;
            uint8_t p : 1;
        } __packed;
    } __packed access_byte;

    union {
        uint8_t raw;

        struct {
            uint8_t limit_high : 4;
            uint8_t avl : 1;
            uint8_t l : 1;
            uint8_t db : 1;
            uint8_t g : 1;
        } __packed;
    } __packed limit_flags;

    uint8_t base_mid_high;
    uint32_t base_high;
    uint32_t : 32;
} __packed;

#endif  // __STRATA_ARCH_GDT_H__
