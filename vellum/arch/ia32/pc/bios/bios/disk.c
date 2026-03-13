#include <vellum/plat/bios/disk.h>

#include <stdint.h>

#include <vellum/plat/bios/bioscall.h>

struct dap {
    uint8_t dap_size;
    uint8_t unused;
    uint16_t count;
    uint16_t buffer_offset;
    uint16_t buffer_segment;
    uint32_t lba_low;
    uint32_t lba_high;
};

#define MAKE_STATUS(code) ((code) ? (0xA0001300 | (code)) : STATUS_SUCCESS)

status_t VlBiosP_ResetDisk(uint8_t drive)
{
    struct bioscall_regs regs = {.a.b.h = 0x00, .d.b.l = drive};

    if (VlBiosP_Call(0x13, &regs) || regs.a.b.h) {
        return MAKE_STATUS(regs.a.b.h);
    }

    return STATUS_SUCCESS;
}

status_t VlBiosP_ReadDisk(uint8_t drive, struct chs chs, uint8_t count, void *buf, uint8_t *result)
{
    struct bioscall_regs regs = {
        .a.b.h = 0x02,
        .a.b.l = count,
        .c.b.h = chs.cylinder & 0xFF,
        .c.b.l = (((chs.cylinder >> 8) & 0x3) << 6) | (chs.sector & 0x3F),
        .d.b.h = chs.head,
        .d.b.l = drive,
        .es.w = ((uintptr_t)buf >> 4) & 0xFFFF,
        .b.w = (uintptr_t)buf & 0x000F,
    };

    if (VlBiosP_Call(0x13, &regs) || regs.a.b.h) {
        return MAKE_STATUS(regs.a.b.h);
    }

    if (result) {
        *result = regs.a.b.l;
    }

    return STATUS_SUCCESS;
}

status_t VlBiosP_WriteDisk(
    uint8_t drive, struct chs chs, uint8_t count, const void *buf, uint8_t *result
)
{
    struct bioscall_regs regs = {
        .a.b.h = 0x03,
        .a.b.l = count,
        .c.b.h = chs.cylinder & 0xFF,
        .c.b.l = (((chs.cylinder >> 8) & 0x3) << 6) | (chs.sector & 0x3F),
        .d.b.h = chs.head,
        .d.b.l = drive,
        .es.w = ((uintptr_t)buf >> 4) & 0xFFFF,
        .b.w = (uintptr_t)buf & 0x000F,
    };

    if (VlBiosP_Call(0x13, &regs) || regs.a.b.h) {
        return MAKE_STATUS(regs.a.b.h);
    }

    if (result) {
        *result = regs.a.b.l;
    }

    return STATUS_SUCCESS;
}

status_t VlBiosP_GetDiskParams(
    uint8_t drive,
    uint8_t *hdd_count,
    uint8_t *type,
    struct chs *geometry,
    struct bios_disk_base_table **dbt
)
{
    struct bioscall_regs regs = {
        .a.b.h = 0x08,
        .d.b.l = drive,
        .es.w = 0,
        .di.w = 0,
    };

    if (VlBiosP_Call(0x13, &regs) || regs.a.b.h) {
        return MAKE_STATUS(regs.a.b.h);
    }

    if (hdd_count) {
        *hdd_count = regs.d.b.l;
    }
    if (type) {
        *type = regs.b.b.l;
    }
    if (geometry) {
        geometry->head = (int)regs.d.b.h + 1;
        geometry->cylinder = (((int)regs.c.b.l >> 6) | (int)regs.c.b.h) + 1;
        geometry->sector = (int)regs.c.b.l & 0x3F;
    }
    if (dbt && !(drive & 0x80)) {
        *dbt = (void *)(((uintptr_t)regs.es.w << 4) + regs.di.w);
    }

    return STATUS_SUCCESS;
}

status_t VlBiosP_CheckDiskExtension(
    uint8_t drive, uint8_t *edd_version, uint16_t *subset_support_flags
)
{
    struct bioscall_regs regs = {
        .a.b.h = 0x41,
        .b.w = 0x55AA,
        .d.b.l = drive,
    };

    if (VlBiosP_Call(0x13, &regs)) {
        return MAKE_STATUS(regs.a.b.h);
    }

    if (regs.b.w != 0xAA55) {
        return STATUS_NOT_SUPPORTED;
    }

    if (edd_version) {
        *edd_version = regs.a.b.h;
    }
    if (subset_support_flags) {
        *subset_support_flags = regs.c.w;
    }

    return 0;
}

status_t VlBiosP_ReadDiskExtended(uint8_t drive, lba_t lba, uint16_t count, void *buf)
{
    struct dap dap = {
        .dap_size = sizeof(struct dap),
        .count = count,
        .buffer_offset = (uintptr_t)buf & 0x000F,
        .buffer_segment = ((uintptr_t)buf >> 4) & 0xFFFF,
        .lba_low = lba & 0xFFFFFFFF,
        .lba_high = lba >> 32,
    };

    struct bioscall_regs regs = {
        .a.b.h = 0x42,
        .d.b.l = drive,
        .ds.w = ((uintptr_t)&dap >> 4) & 0xFFFF,
        .si.w = (uintptr_t)&dap & 0x000F,
    };

    if (VlBiosP_Call(0x13, &regs) || regs.a.b.h) {
        return MAKE_STATUS(regs.a.b.h);
    }

    return STATUS_SUCCESS;
}

status_t VlBiosP_WriteDiskExtended(uint8_t drive, lba_t lba, uint16_t count, const void *buf)
{
    struct dap dap = {
        .dap_size = sizeof(struct dap),
        .count = count,
        .buffer_offset = (uintptr_t)buf & 0x000F,
        .buffer_segment = ((uintptr_t)buf >> 4) & 0xFFFF,
        .lba_low = lba & 0xFFFFFFFF,
        .lba_high = lba >> 32,
    };

    struct bioscall_regs regs = {
        .a.b.h = 0x43,
        .d.b.l = drive,
        .ds.w = ((uintptr_t)&dap >> 4) & 0xFFFF,
        .si.w = (uintptr_t)&dap & 0x000F,
    };

    if (VlBiosP_Call(0x13, &regs) || regs.a.b.h) {
        return MAKE_STATUS(regs.a.b.h);
    }

    return STATUS_SUCCESS;
}

status_t VlBiosP_GetDiskParamsExtended(uint8_t drive, struct bios_extended_drive_params *params)
{
    struct bioscall_regs regs = {
        .a.b.h = 0x48,
        .d.b.l = drive,
        .ds.w = ((uintptr_t)params >> 4) & 0xFFFF,
        .si.w = (uintptr_t)params & 0x000F,
    };

    if (VlBiosP_Call(0x13, &regs) || regs.a.b.h) {
        return MAKE_STATUS(regs.a.b.h);
    }

    return STATUS_SUCCESS;
}
