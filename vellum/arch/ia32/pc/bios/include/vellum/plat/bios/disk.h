#ifndef __VELLUM_ASM_BIOS_DISK_H__
#define __VELLUM_ASM_BIOS_DISK_H__

#include <stdint.h>

#include <vellum/arch/farptr.h>

#include <vellum/disk.h>
#include <vellum/status.h>

#define DRIVE_TYPE_360K      0x00
#define DRIVE_TYPE_1_2M      0x01
#define DRIVE_TYPE_720k      0x02
#define DRIVE_TYPE_1_44M     0x03
#define DRIVE_TYPE_2_88M_ALT 0x04
#define DRIVE_TYPE_2_88M     0x05
#define DRIVE_TYPE_ATAPI     0x10

struct bios_disk_base_table {
    uint8_t data[11];
};

#define EXT_FEATURE_PACKET     0x0001
#define EXT_FEATURE_LOCK_EJECT 0x0002
#define EXT_FEATURE_EDD        0x0004

#define EDD_VER_1_X 0x01
#define EDD_VER_2_0 0x20
#define EDD_VER_2_1 0x21
#define EDD_VER_3_0 0x30

struct bios_extended_drive_params {
    uint16_t table_size;
    uint16_t flags;
    uint32_t geom_cylinders;
    uint32_t geom_heads;
    uint32_t geom_sectors;
    uint64_t total_sectors;
    uint16_t bytes_per_sector;

    struct VlA_FarPtr16 edd_config_params;

    uint16_t device_path_signature;
    uint8_t device_path_size;
    uint8_t reserved1[3];
    char host_bus[4];
    char interface[8];
    char interface_path[8];
    char device_path[8];
    uint8_t reserved2;
    uint8_t device_path_checksum;
};

status_t VlBiosP_ResetDisk(uint8_t drive);

status_t VlBiosP_ReadDisk(uint8_t drive, struct chs chs, uint8_t count, void *buf, uint8_t *result);

status_t VlBiosP_WriteDisk(
    uint8_t drive, struct chs chs, uint8_t count, const void *buf, uint8_t *result
);

status_t VlBiosP_GetDiskParams(
    uint8_t drive,
    uint8_t *hdd_count,
    uint8_t *type,
    struct chs *geometry,
    struct bios_disk_base_table **dbt
);

status_t VlBiosP_CheckDiskExtension(
    uint8_t drive, uint8_t *edd_version, uint16_t *subset_support_flags
);

status_t VlBiosP_ReadDiskExtended(uint8_t drive, lba_t lba, uint16_t count, void *buf);

status_t VlBiosP_WriteDiskExtended(uint8_t drive, lba_t lba, uint16_t count, const void *buf);

status_t VlBiosP_GetDiskParamsExtended(uint8_t drive, struct bios_extended_drive_params *params);

#endif  // __VELLUM_ASM_BIOS_DISK_H__
