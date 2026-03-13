#include "mbr.h"

#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/device.h>
#include <vellum/interface/block.h>
#include <vellum/macros.h>
#include <vellum/status.h>

struct mbr_data {
    struct device *blkdev;
    const struct block_interface *blkif;
};

static status_t get_block_size(struct device *dev, size_t *size)
{
    struct mbr_data *data = (struct mbr_data *)dev->data;

    return data->blkif->get_block_size(data->blkdev, size);
}

static status_t read(struct device *dev, lba_t lba, void *buf, size_t count, size_t *result)
{
    struct mbr_data *data = (struct mbr_data *)dev->data;

    return data->blkif->read(data->blkdev, lba, buf, count, result);
}

static status_t write(struct device *dev, lba_t lba, const void *buf, size_t count, size_t *result)
{
    struct mbr_data *data = (struct mbr_data *)dev->data;

    return data->blkif->write(data->blkdev, lba, buf, count, result);
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

static void mbr_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"mbr\"");
    }

    drv->name = "mbr";
    drv->probe = probe;
    drv->remove = remove;
    drv->get_interface = get_interface;
}

static status_t register_partitions(struct device *dev, struct device_driver *partdrv, lba_t base)
{
    status_t status;
    struct mbr_data *data = (struct mbr_data *)dev->data;
    uint8_t buf[512];
    struct mbr *mbr_sect = NULL;
    struct mbr_partition_entry *entry = NULL;
    struct device *partdev = NULL;
    uint32_t base_lba, sector_count;

    mbr_sect = (struct mbr *)buf;
    status = data->blkif->read(data->blkdev, base, buf, 1, NULL);
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (le16toh(mbr_sect->boot_signature) != MBR_SIGNATURE) {
        status = STATUS_INVALID_SIGNATURE;
        goto has_error;
    }
    if (mbr_sect->partition_entries[0].type == MBR_PART_TYPE_GPT) {
        status = STATUS_INVALID_VALUE;
        goto has_error;
    }

    for (int i = 0; i < 4; i++) {
        entry = &mbr_sect->partition_entries[i];
        if (!entry->type) continue;

        base_lba = le32toh(entry->base_lba);
        sector_count = le32toh(entry->sector_count);

        if (entry->type == MBR_PART_TYPE_EXTENDED) {
            status = register_partitions(dev, partdrv, base + base_lba);
            if (!CHECK_SUCCESS(status)) goto has_error;
        } else {
            struct resource res[] = {
                {
                    .type = RT_LBA,
                    .base = base + base_lba,
                    .limit = base + base_lba + sector_count - 1,
                    .flags = 0,
                },
            };

            status = partdrv->probe(&partdev, partdrv, dev, res, ARRAY_SIZE(res));
            if (!CHECK_SUCCESS(status)) goto has_error;
        }
    }

    return STATUS_SUCCESS;

has_error:
    return status;
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
    struct device *dev = NULL;
    struct device *blkdev = NULL;
    const struct block_interface *blkif = NULL;
    char dev_name_base[sizeof(dev->name)];
    struct mbr_data *data = NULL;
    struct device_driver *partdrv = NULL;

    blkdev = parent;
    if (!blkdev) {
        status = STATUS_INVALID_VALUE;
        goto has_error;
    }

    status = blkdev->driver->get_interface(blkdev, "block", (const void **)&blkif);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_Create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    snprintf(dev_name_base, sizeof(dev_name_base), "%.61spt", parent->name);

    status = VlDev_GenerateName(dev_name_base, dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->blkdev = blkdev;
    data->blkif = blkif;
    dev->data = data;

    status = VlDev_FindDriver("part", &partdrv);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = register_partitions(dev, partdrv, 0);
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (devout) *devout = dev;

    return STATUS_SUCCESS;

has_error:
    if (dev) {
        for (struct device *partdev = dev->first_child; partdev; partdev = partdev->sibling) {
            partdev->driver->remove(partdev);
        }
    }

    if (data) {
        free(data);
    }

    if (dev) {
        VlDev_Remove(dev);
    }

    return status;
}

static status_t remove(struct device *dev)
{
    struct mbr_data *data = (struct mbr_data *)dev->data;

    for (struct device *partdev = dev->first_child; partdev; partdev = partdev->sibling) {
        partdev->driver->remove(partdev);
    }

    free(data);

    VlDev_Remove(dev);

    return STATUS_SUCCESS;
}

static status_t get_interface(struct device *dev, const char *name, const void **result)
{
    if (strcmp(name, "block") == 0) {
        if (result) *result = &blkif;
        return STATUS_SUCCESS;
    }

    return STATUS_ENTRY_NOT_FOUND;
}

REGISTER_DEVICE_DRIVER(mbr, mbr_init)
