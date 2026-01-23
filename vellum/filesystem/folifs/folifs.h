#ifndef __FOLIFS_H__
#define __FOLIFS_H__

#include <stdint.h>

#include <vellum/compiler.h>

#define FOLIFS_VBR_SIGNATURE   0xAA55
#define FOLIFS_FS_SIGNATURE    "FOLI"

typedef struct {
    uint8_t bytes[16];
} __packed folifs_uuid_t;

struct folifs_first_sector {
    uint8_t bootcode[496];
    uint64_t volume_offset;
    char filesystem_signature[4];
    uint16_t reserved_sectors;
    uint16_t vbr_signature;
} __packed;

struct folifs_rdb {
    uint64_t total_sector_count;
    uint64_t total_block_count;
    uint8_t sectors_per_block;
    uint8_t rdb_copy_count;
    uint16_t bytes_per_sector;
    uint32_t flags;
    uint64_t root_mdb_pointer;
    uint64_t udb_pointer;
    uint64_t jbb_pointer;
    uint64_t rbb_pointer;
    uint64_t group0_gbb_pointer;
    folifs_uuid_t volume_uuid;
    char formatted_os[16];
    uint16_t filesystem_version;
} __packed;

struct folifs_mdb {
    uint64_t parent_mdb_pointer;
    uint64_t next_mdb_pointer;
    uint32_t flags;
    uint32_t attribute_entry_presence_bitmap;
    uint64_t file_size;
    uint64_t dhb_ddb_pointer;
    uint8_t reserved[8];
} __packed;

struct folifs_mdb_attribute_entry_header {
    uint32_t entry_length;
    uint8_t entry_type;
    uint8_t reserved[3];
} __packed;

enum folifs_mdb_entry_type {
    MDB_ENTRY_FAE = 1,
    MDB_ENTRY_TAE,
    MDB_ENTRY_PAE,
    MDB_ENTRY_QAE,
    MDB_ENTRY_EAE,
    MDB_ENTRY_CAE,
};

struct folifs_mdb_fae {
    struct folifs_mdb_attribute_entry_header header;
    uint32_t filename_hash;
    uint8_t reserved[4];
    uint8_t filename_length;
    char filename[255];
} __packed;

struct folifs_mdb_tae {
    struct folifs_mdb_attribute_entry_header header;
    uint64_t create_time;
    uint64_t write_time;
    uint64_t read_time;
} __packed;

struct folifs_mdb_pae {
    struct folifs_mdb_attribute_entry_header header;
    uint64_t acb_pointer;
} __packed;

struct folifs_mdb_qae {
    struct folifs_mdb_attribute_entry_header header;
    uint64_t qcb_pointer;
} __packed;

struct folifs_mdb_eae {
    struct folifs_mdb_attribute_entry_header header;
    uint8_t encryption_algorithm;
    uint8_t reserved[15];
    uint8_t decription_test_data[];
} __packed;

struct folifs_mdb_cae {
    struct folifs_mdb_attribute_entry_header header;
    uint8_t compression_algorithm;
    uint8_t reserved[15];
} __packed;

struct folifs_acb_entry {
    folifs_uuid_t uuid;
    uint16_t permission_bitmap;
    uint16_t flags;
    uint8_t reserved[12];
} __packed;

struct folifs_acb {
    uint64_t parent_acb_head_pointer;
    uint64_t next_acb_pointer;
    uint16_t entry_count;
    uint8_t reserved[6];
    struct folifs_acb_entry entries[];
} __packed;

struct folifs_qcb_entry {
    folifs_uuid_t uuid;
    uint64_t current_file_count;
    uint64_t file_count_limit;
    uint64_t block_count_soft_limit;
    uint64_t block_count_hard_limit;
    uint64_t current_block_count;
    uint64_t last_limit_alarm_time;
    uint32_t flags;
    uint8_t reserved[4];
} __packed;

struct folifs_qcb {
    uint64_t next_qcb_pointer;
    uint16_t entry_count;
    uint8_t reserved[14];
    struct folifs_qcb_entry entries[];
} __packed;

struct folifs_dhb_entry {
    uint64_t mdb_pointer;
    uint32_t filename_hash;
    uint8_t reserved[6];
    uint16_t left_child_offset;
    uint16_t right_child_offset;
    uint16_t next_child_offset;
    uint64_t left_child_block;
    uint64_t right_child_block;
    uint64_t next_child_block;
} __packed;

struct folifs_dhb {
    uint16_t entry_count;
    uint8_t reserved[14];
    struct folifs_dhb_entry entries[];
} __packed;

struct folifs_ddb {
    uint64_t previous_ddb_pointer;
    uint64_t next_ddb_pointer;
    uint64_t dsb_pointers[];
} __packed;

#endif // __FOLIFS_H__
