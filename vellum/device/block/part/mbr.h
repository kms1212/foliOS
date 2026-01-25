#ifndef __DEVICE_BLOCK_PART_MBR_H__
#define __DEVICE_BLOCK_PART_MBR_H__

#include <stdint.h>

#include <vellum/compiler.h>

#define MBR_SIGNATURE 0xAA55

#define MBR_PART_TYPE_GPT      0xEE
#define MBR_PART_TYPE_EXTENDED 0x05

struct mbr_partition_entry {
    uint8_t status;
    uint8_t base_chs[3];
    uint8_t type;
    uint8_t limit_chs[3];
    uint32_t base_lba;
    uint32_t sector_count;
} __packed;

struct mbr {
    uint8_t bootcode[446];
    struct mbr_partition_entry partition_entries[4];
    uint16_t boot_signature;
} __packed;

#endif  // __DEVICE_BLOCK_PART_MBR_H__
