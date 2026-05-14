#ifndef __LOADST_BOOTINFO_H__
#define __LOADST_BOOTINFO_H__

#include <stdint.h>

#include <loadst/compiler.h>

#define BTV_CURRENT 0

#define BTF_BIGENDIAN 0x0001

struct bootinfo_table_header {
    uint16_t flags;
    uint16_t version;
    uint32_t header_size;
    uint16_t entry_count;
    uint16_t reserved;
    uint32_t size;
    char strtab[];
} __packed;

#define BEF_REQUIRED 0x0001

struct bootinfo_entry_header {
    uint32_t type;
    uint16_t flags;
    uint16_t header_size;
    uint32_t size;
} __packed;

#define BET_COMMAND_ARGS       0
#define BET_LOADER_INFO        1
#define BET_MEMORY_MAP         2
#define BET_SYSTEM_DISK        3
#define BET_ACPI_RSDP          4
#define BET_FRAMEBUFFER        5
#define BET_DEFAULT_FONT       6
#define BET_BOOT_GRAPHICS      7
#define BET_UNAVAILABLE_FRAMES 8
#define BET_PAGETABLE_VPN      9
#define BET_RAMDISK            10

struct bootinfo_entry_command_args {
    uint32_t arg_count;
    uint32_t arg_offsets[];
} __packed;

struct bootinfo_entry_loader_info {
    uint32_t additional_entry_count;
    uint32_t name_offset;
    uint32_t version_offset;
    uint32_t author_offset;
    uint32_t additional_entries[];
} __packed;

#define BEMT_FREE             1
#define BEMT_RESERVED         2
#define BEMT_ACPI_RECLAIMABLE 3
#define BEMT_ACPI_NVS         4
#define BEMT_BAD              5

struct bootinfo_entry_memory_map {
    uint32_t entry_count;
    struct bootinfo_memory_map_entry {
        uint64_t base;
        uint64_t size;
        uint32_t type;
        uint32_t reserved;
    } __packed entries[];
} __packed;

struct bootinfo_entry_system_disk {
    uint32_t ident_crc32;
    uint32_t entry_count;
    struct bootinfo_system_disk_entry {
        uint64_t lba;
        uint32_t crc32;
    } __packed entries[];
} __packed;

struct bootinfo_entry_acpi_rsdp {
    char oemid[6];
    uint8_t revision;
    uint8_t reserved;
    uint32_t size;
    uint32_t rsdt_addr;
    uint64_t xsdt_addr;
} __packed;

#define BEFT_TEXT   0
#define BEFT_DIRECT 1

struct bootinfo_entry_framebuffer {
    uint64_t framebuffer_addr;
    uint32_t width;
    uint32_t pitch;
    uint32_t height;
    uint8_t bpp;
    uint8_t type;
    uint16_t reserved;
    union {
        struct bootinfo_framebuffer_direct_color_info {
            uint8_t red_pos;
            uint8_t red_size;
            uint8_t green_pos;
            uint8_t green_size;
            uint8_t blue_pos;
            uint8_t blue_size;
        } __packed direct;
    } __packed;
} __packed;

struct bootinfo_entry_default_font {
    uint8_t width;
    uint8_t height;
    uint8_t font_bpp;
    uint64_t data_addr;
} __packed;

struct bootinfo_entry_boot_graphics {
    uint16_t width;
    uint16_t height;
    uint8_t bpp;
    uint64_t data_addr;
} __packed;

#define BEUT_PAGETABLE     0
#define BEUT_KERNEL        1
#define BEUT_DEFAULT_FONT  2
#define BEUT_BOOT_GRAPHICS 3
#define BEUT_RAMDISK       4

struct bootinfo_entry_unavailable_frames {
    uint32_t entry_count;
    struct bootinfo_unavailable_frame_entry {
        uint64_t pfn_base;
        uint32_t count;
        uint32_t type;
    } __packed entries[];
} __packed;

struct bootinfo_entry_pagetable_vpn {
    uint64_t vpn;
} __packed;

struct bootinfo_entry_ramdisk {
    uint8_t version;
    uint8_t reserved[3];
    uint32_t size;
    uint64_t data_addr;
} __packed;

#endif  // __LOADST_BOOTINFO_H__
