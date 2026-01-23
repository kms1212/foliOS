#ifndef __DEVICE_BLOCK_PART_GPT_H__
#define __DEVICE_BLOCK_PART_GPT_H__

#include "mbr.h"

#define GPT_HEADER_SIGNATURE    "EFI PART"

typedef uint8_t gpt_guid[16];

struct gpt_header {
    char signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t partition_base_lba;
    uint64_t partition_limit_lba;
    gpt_guid disk_guid;
    uint64_t entry_list_base_lba;
    uint32_t entry_count;
    uint32_t bytes_per_entry;
    uint32_t entry_list_crc32;
} __packed;

struct gpt_partition_entry {
    gpt_guid type;
    gpt_guid uid;
    uint64_t base_lba;
    uint64_t limit_lba;
    uint64_t flags;
    uint16_t name[72];
} __packed;

#endif // __DEVICE_BLOCK_PART_GPT_H__
