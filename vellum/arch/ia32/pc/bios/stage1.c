#include <vellum/plat/bios/bootinfo.h>
#include <vellum/plat/bios/disk.h>
#include <vellum/plat/bios/video.h>

#include <stdint.h>
#include <string.h>

#include <vellum/compiler.h>
#include <vellum/disk.h>

#include "../../filesystem/fat/fat.h"

#ifdef NDEBUG
#    define PRINT_STR(str)
#    define PRINT_HEX(val)

#else
#    define PRINT_STR(str) print_str(str)
#    define PRINT_HEX(val) print_hex(val)

#endif

extern char __stage2_start[];  // NOLINT(bugprone-reserved-identifier)

static uint8_t sect_buf[4096] __aligned(16);
static uint8_t clus_buf[4096] __aligned(16);
static struct fat_bpb_sector *bpb = (struct fat_bpb_sector *)0x7C00;
static struct chs geom;

int memcmp(const void *p1, const void *p2, size_t len)
{
    const uint8_t *a = p1, *b = p2;
    while (len--) {
        if (*a != *b) return (*a - *b);
        a++;
        b++;
    }
    return 0;
}

static void print_str(const char *str)
{
    while (*str) {
        VlBiosP_WriteVideoTty(*str++);
    }
}

static void print_hex(uint32_t val)
{
    char hex_str[9];

    for (int i = 7; i >= 0; i--) {
        hex_str[i] = "0123456789ABCDEF"[val & 0xF];
        val >>= 4;
    }

    hex_str[8] = 0;

    print_str(hex_str);
}

static status_t read_disk(lba_t lba, uint8_t count, void *buf)
{
    status_t status;
    struct chs chs;
    uint8_t result;

    lba += bpb->hidden_sector_count;

    chs.cylinder = (int)(lba / (lba_t)(geom.head * geom.sector));
    chs.head = (int)((lba / geom.sector) % geom.head);
    chs.sector = (int)((lba % geom.sector) + 1);

    status = VlBiosP_ReadDisk(_pc_boot_drive, chs, count, buf, &result);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

void s1main(void)
{
    status_t status;

    PRINT_STR("[stage1] getting disk parameters...\r\n");
    status = VlBiosP_GetDiskParams(_pc_boot_drive, NULL, NULL, &geom, NULL);
    if (!CHECK_SUCCESS(status)) {
        PRINT_STR("[stage1] failed to request disk geometry: ");
        PRINT_HEX(status);
        return;
    }

    uint16_t fat_lba = bpb->reserved_sector_count;
    uint32_t rootdir_lba = fat_lba + bpb->fat_size16 * bpb->fat_count;
    uint32_t cluster_start_lba = rootdir_lba + (bpb->root_entry_count * 32 + 511) / 512;
    uint16_t bytes_per_cluster = bpb->bytes_per_sector * bpb->sectors_per_cluster;

    union fat_dir_entry *entry = NULL;
    PRINT_STR("[stage1] searching root directory...\r\n");
    for (lba_t current_lba = rootdir_lba; current_lba < cluster_start_lba; current_lba++) {
        status = read_disk(current_lba, 1, sect_buf);
        if (!CHECK_SUCCESS(status)) {
            PRINT_STR("[stage1] failed to read disk: ");
            PRINT_HEX(status);
            return;
        }

        for (int i = 0; i < 32; i++) {
            entry = &((union fat_dir_entry *)sect_buf)[i];

            if (memcmp(
                    entry->file.name_ext,
                    "VELLUM  X86",
                    sizeof(entry->file.name) + sizeof(entry->file.extension)
                ) == 0 ||
                !entry->file.name[0]) {
                goto file_found;
            }
        }
    }

file_found: {
}
    if (!entry) {
        PRINT_STR("[stage1] VELLUM.X86 not found");
        return;
    }

    uint16_t current_cluster = entry->file.cluster_location;

    uint32_t *load_dest = (uint32_t *)__stage2_start;

    PRINT_STR("[stage1] reading FAT area...\r\n");
    status = read_disk(fat_lba, 8, sect_buf);
    if (!CHECK_SUCCESS(status)) {
        PRINT_STR("[stage1] failed to read disk: ");
        PRINT_HEX(status);
        return;
    }
    PRINT_STR("[stage1] loading file...\r\n");
    while (current_cluster <= FAT12_MAX_CLUSTER) {
        status = read_disk(
            cluster_start_lba + (current_cluster - 2) * bpb->sectors_per_cluster,
            bpb->sectors_per_cluster,
            clus_buf
        );
        if (!CHECK_SUCCESS(status)) {
            PRINT_STR("\r\n[stage1] failed to read disk: ");
            PRINT_HEX(status);
            return;
        }

        for (int i = 0; i < bytes_per_cluster / sizeof(uint32_t); i++) {
            *load_dest++ = ((uint32_t *)clus_buf)[i];
        }
        print_str(".");

        if (current_cluster & 1) {
            current_cluster = (sect_buf[current_cluster * 3 / 2 + 1] << 4) |
                (sect_buf[current_cluster * 3 / 2] >> 4);
        } else {
            current_cluster = ((sect_buf[current_cluster * 3 / 2 + 1] & 0xF) << 8) |
                sect_buf[current_cluster * 3 / 2];
        }
    }
    PRINT_STR("\r\n");

    void (*stage2_entry)(void);
    *(void **)&stage2_entry = (void *)__stage2_start;

    stage2_entry();

    return;
}
