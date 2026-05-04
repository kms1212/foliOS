#ifndef __STRATA_PLAT_BIOS_H__
#define __STRATA_PLAT_BIOS_H__

#include <stdint.h>

#include <strata/arch/farptr.h>

#include <strata/compiler.h>
#include <strata/macros.h>
#include <strata/mm/types.h>
#include <strata/status.h>

struct StBiosP_Bda {
    struct StA_FarPtr16 ivt[256];
    uint8_t bios_stack[256];
    uint16_t com_io_addr[4];
    uint16_t lpt_io_addr[3];
    uint16_t ebda_segment;
    uint16_t equipment_list;
    uint8_t pcjr_ir_kb_link_err_count;
    uint16_t base_mem_size_kb;
    RESERVE_2BYTES;
    uint16_t ps2_bios_ctrl_flags;
    uint16_t keyboard_flags;
    uint8_t alt_keypad_entry;
    uint16_t kb_buffer_head_offset;
    uint16_t kb_buffer_tail_offset;

    // TBD
} __packed;

struct StBiosP_ExtendedBda {
    uint16_t ebda_size_kb;

    // TBD
} __packed;

#endif  // __STRATA_PLAT_BIOS_H__
