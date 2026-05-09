#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/plat/bios/disk.h>

#include <vellum/device.h>
#include <vellum/interface/block.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/status.h>

#define MODULE_NAME "biosdisk"

struct biosdisk_data {
    uint8_t drive;
    uint8_t bios_type;
    int is_fixed;
    int use_packet;
    size_t block_size;
    lba_t total_blocks;
    struct chs geometry;
};

static status_t get_block_size(struct device *dev, size_t *size)
{
    struct biosdisk_data *data = (struct biosdisk_data *)dev->data;

    if (size) *size = data->block_size;

    return STATUS_SUCCESS;
}

static status_t read_chunk(struct biosdisk_data *data, lba_t lba, size_t count)
{
    if (data->use_packet) {
        return VlBiosP_ReadDiskExtended(
            data->drive,
            lba,
            (uint16_t)count,
            _pc_bios_disk_transfer_buffer
        );
    }

    struct chs chs = VlDisk_LbaToChs(lba, data->geometry);
    uint8_t result;

    return VlBiosP_ReadDisk(data->drive, chs, 1, _pc_bios_disk_transfer_buffer, &result);
}

static status_t write_chunk(struct biosdisk_data *data, lba_t lba, size_t count)
{
    if (data->use_packet) {
        return VlBiosP_WriteDiskExtended(
            data->drive,
            lba,
            (uint16_t)count,
            _pc_bios_disk_transfer_buffer
        );
    }

    struct chs chs = VlDisk_LbaToChs(lba, data->geometry);
    uint8_t result;

    return VlBiosP_WriteDisk(data->drive, chs, 1, _pc_bios_disk_transfer_buffer, &result);
}

static status_t read(struct device *dev, lba_t lba, void *buf, size_t count, size_t *result)
{
    status_t status;
    struct biosdisk_data *data = (struct biosdisk_data *)dev->data;
    size_t done = 0;
    uint8_t *out = buf;
    size_t max_transfer_count;

    if (!buf && count) return STATUS_INVALID_VALUE;
    if (lba < 0) return STATUS_INVALID_VALUE;

    if (result) *result = 0;
    if (!count) return STATUS_SUCCESS;

    if (data->total_blocks && (uint64_t)lba >= (uint64_t)data->total_blocks) {
        return STATUS_INVALID_VALUE;
    }
    if (data->total_blocks && (uint64_t)lba + count > (uint64_t)data->total_blocks) {
        count = (size_t)((uint64_t)data->total_blocks - (uint64_t)lba);
    }

    if (data->block_size == 0 || data->block_size > BIOS_DISK_TRANSFER_BUFFER_SIZE) {
        return STATUS_NOT_SUPPORTED;
    }

    max_transfer_count = BIOS_DISK_TRANSFER_BUFFER_SIZE / data->block_size;
    if (!max_transfer_count) return STATUS_NOT_SUPPORTED;

    while (done < count) {
        size_t current_count = MIN(count - done, max_transfer_count);
        size_t byte_count;

        if (!data->use_packet) current_count = 1;

        status = read_chunk(data, lba + (lba_t)done, current_count);
        if (!CHECK_SUCCESS(status)) return status;

        byte_count = current_count * data->block_size;
        memcpy(out + done * data->block_size, _pc_bios_disk_transfer_buffer, byte_count);

        done += current_count;
    }

    if (result) *result = done;

    return STATUS_SUCCESS;
}

static status_t write(struct device *dev, lba_t lba, const void *buf, size_t count, size_t *result)
{
    status_t status;
    struct biosdisk_data *data = (struct biosdisk_data *)dev->data;
    const uint8_t *in = buf;
    size_t done = 0;
    size_t max_transfer_count;

    if (!buf && count) return STATUS_INVALID_VALUE;
    if (lba < 0) return STATUS_INVALID_VALUE;

    if (result) *result = 0;
    if (!count) return STATUS_SUCCESS;

    if (data->total_blocks && (uint64_t)lba >= (uint64_t)data->total_blocks) {
        return STATUS_INVALID_VALUE;
    }
    if (data->total_blocks && (uint64_t)lba + count > (uint64_t)data->total_blocks) {
        count = (size_t)((uint64_t)data->total_blocks - (uint64_t)lba);
    }

    if (data->block_size == 0 || data->block_size > BIOS_DISK_TRANSFER_BUFFER_SIZE) {
        return STATUS_NOT_SUPPORTED;
    }

    max_transfer_count = BIOS_DISK_TRANSFER_BUFFER_SIZE / data->block_size;
    if (!max_transfer_count) return STATUS_NOT_SUPPORTED;

    while (done < count) {
        size_t current_count = MIN(count - done, max_transfer_count);
        size_t byte_count;

        if (!data->use_packet) current_count = 1;

        byte_count = current_count * data->block_size;
        memcpy(_pc_bios_disk_transfer_buffer, in + done * data->block_size, byte_count);

        status = write_chunk(data, lba + (lba_t)done, current_count);
        if (!CHECK_SUCCESS(status)) return status;

        done += current_count;
    }

    if (result) *result = done;

    return STATUS_SUCCESS;
}

static const struct block_interface blkif = {
    .get_block_size = get_block_size,
    .read = read,
    .write = write,
};

static status_t probe(
    struct device **devout,
    struct device_driver *drv,
    struct device *parent,
    struct resource *rsrc,
    int rsrc_cnt
);
static status_t remove(struct device *dev);
static status_t get_interface(struct device *dev, const char *name, const void **result);

static void biosdisk_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"biosdisk\"");
    }

    drv->name = "biosdisk";
    drv->probe = probe;
    drv->remove = remove;
    drv->get_interface = get_interface;
}

static void detect_partitions(struct device *dev)
{
    status_t status;
    struct device *ptdev = NULL;
    struct device_driver *ptdrv = NULL;

    status = VlDev_FindDriver("mbr", &ptdrv);
    if (CHECK_SUCCESS(status)) {
        status = ptdrv->probe(&ptdev, ptdrv, dev, NULL, 0);
        if (CHECK_SUCCESS(status)) return;
    }

    status = VlDev_FindDriver("gpt", &ptdrv);
    if (CHECK_SUCCESS(status)) {
        (void)ptdrv->probe(&ptdev, ptdrv, dev, NULL, 0);
    }
}

static status_t probe(
    struct device **devout,
    struct device_driver *drv,
    struct device *parent,
    struct resource *rsrc,
    int rsrc_cnt
)
{
    status_t status;
    uint8_t edd_version;
    uint16_t edd_features;
    struct bios_extended_drive_params edd_params = {0};
    struct biosdisk_data *data = NULL;
    struct device *dev = NULL;

    if (!rsrc || rsrc_cnt != 1 || rsrc[0].type != RT_BUS || rsrc[0].base != rsrc[0].limit ||
        rsrc[0].base > UINT8_MAX) {
        return STATUS_INVALID_RESOURCE;
    }

    data = calloc(1, sizeof(*data));
    if (!data) return STATUS_UNKNOWN_ERROR;

    data->drive = (uint8_t)rsrc[0].base;
    data->block_size = 512;

    status = VlBiosP_CheckDiskExtension(data->drive, &edd_version, &edd_features);
    if (CHECK_SUCCESS(status) && (edd_features & EXT_FEATURE_PACKET)) {
        data->use_packet = 1;

        status = VlBiosP_GetDiskParamsExtended(data->drive, &edd_params);
        if (CHECK_SUCCESS(status)) {
            if (edd_params.bytes_per_sector) data->block_size = edd_params.bytes_per_sector;
            data->total_blocks = (lba_t)edd_params.total_sectors;
        }
    }

    status = VlBiosP_GetDiskParams(data->drive, NULL, &data->bios_type, &data->geometry, NULL);
    if (!CHECK_SUCCESS(status) && !data->use_packet) goto has_error;

    data->is_fixed = !!(data->drive & 0x80);
    if (data->drive >= 0xE0 || data->bios_type == DRIVE_TYPE_ATAPI || data->block_size == 2048 ||
        (edd_params.table_size >= offsetof(struct bios_extended_drive_params, edd_config_params) &&
         (edd_params.flags & EDD_INFO_REMOVABLE))) {
        data->is_fixed = 0;
    }

    status = VlDev_Create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_GenerateName(data->is_fixed ? "fd" : "rd", dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    dev->data = data;

    LOG_DEBUG(
        "created BIOS disk %s: drive=%02X type=%02X block=%zu packet=%d total=%lld\n",
        dev->name,
        data->drive,
        data->bios_type,
        data->block_size,
        data->use_packet,
        (long long)data->total_blocks
    );

    if (!(rsrc[0].flags & BIOS_DISK_RESOURCE_SKIP_PARTITIONS)) {
        detect_partitions(dev);
    }

    if (devout) *devout = dev;

    return STATUS_SUCCESS;

has_error:
    if (dev) {
        VlDev_Remove(dev);
    }
    free(data);

    return status;
}

static status_t remove(struct device *dev)
{
    struct biosdisk_data *data = (struct biosdisk_data *)dev->data;

    free(data);
    VlDev_Remove(dev);

    return STATUS_SUCCESS;
}

static status_t get_interface(struct device *dev, const char *name, const void **result)
{
    if (strcmp(name, BLOCK_INTERFACE_ID) == 0) {
        if (result) *result = &blkif;
        return STATUS_SUCCESS;
    }

    return STATUS_ENTRY_NOT_FOUND;
}

REGISTER_DEVICE_DRIVER(biosdisk, biosdisk_init)
