#include "gpt.h"

#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/device.h>
#include <vellum/interface/block.h>
#include <vellum/macros.h>
#include <vellum/status.h>

static const gpt_guid null_entry = {
    0,
};

struct gpt_data {
    struct device *blkdev;
    const struct block_interface *blkif;
};

static status_t get_block_size(struct device *dev, size_t *size)
{
    struct gpt_data *data = (struct gpt_data *)dev->data;

    return data->blkif->get_block_size(data->blkdev, size);
}

static status_t read(struct device *dev, lba_t lba, void *buf, size_t count, size_t *result)
{
    struct gpt_data *data = (struct gpt_data *)dev->data;

    return data->blkif->read(data->blkdev, lba, buf, count, result);
}

static status_t write(struct device *dev, lba_t lba, const void *buf, size_t count, size_t *result)
{
    struct gpt_data *data = (struct gpt_data *)dev->data;

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

static void gpt_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"gpt\"");
    }

    drv->name = "gpt";
    drv->probe = probe;
    drv->remove = remove;
    drv->get_interface = get_interface;
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
    struct gpt_data *data = NULL;
    uint8_t sect_buf[512];
    struct mbr *prot_mbr_sect = NULL;
    lba_t gpt_base;
    struct gpt_header *gpt_header = NULL;
    uint32_t bytes_per_entry;
    uint32_t entry_count;
    lba_t entry_list_lba;
    uint32_t offset;
    struct gpt_partition_entry *entry = NULL;
    struct device *partdev = NULL;
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

    prot_mbr_sect = (struct mbr *)sect_buf;
    status = data->blkif->read(data->blkdev, 0, sect_buf, 1, NULL);
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (le16toh(prot_mbr_sect->boot_signature) != MBR_SIGNATURE) {
        status = STATUS_INVALID_SIGNATURE;
        goto has_error;
    }
    if (prot_mbr_sect->partition_entries[0].type != MBR_PART_TYPE_GPT) {
        status = STATUS_INVALID_VALUE;
        goto has_error;
    }

    gpt_base = le32toh(prot_mbr_sect->partition_entries[0].base_lba);

    gpt_header = (struct gpt_header *)sect_buf;
    status = data->blkif->read(data->blkdev, gpt_base, sect_buf, 1, NULL);
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (strncmp(gpt_header->signature, GPT_HEADER_SIGNATURE, sizeof(gpt_header->signature)) != 0) {
        status = STATUS_INVALID_SIGNATURE;
        goto has_error;
    }

    bytes_per_entry = gpt_header->bytes_per_entry;
    entry_count = gpt_header->entry_count;
    entry_list_lba = le64toh(gpt_header->entry_list_base_lba);

    status = data->blkif->read(data->blkdev, entry_list_lba, sect_buf, 1, NULL);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_FindDriver("part", &partdrv);
    if (!CHECK_SUCCESS(status)) goto has_error;

    offset = 0;
    for (int i = 0; i < entry_count; i++) {
        entry = (struct gpt_partition_entry *)&sect_buf[offset];
        if (memcmp(entry->type, null_entry, sizeof(entry->type)) == 0) break;

        struct resource res[] = {
            {
                .type = RT_LBA,
                .base = entry->base_lba,
                .limit = entry->limit_lba,
                .flags = 0,
            },
        };

        status = partdrv->probe(&partdev, partdrv, dev, res, ARRAY_SIZE(res));
        if (!CHECK_SUCCESS(status)) goto has_error;

        offset += bytes_per_entry;
        if (offset >= sizeof(sect_buf)) {
            offset -= sizeof(sect_buf);
            entry_list_lba++;
            blkif->read(data->blkdev, entry_list_lba, sect_buf, 1, NULL);
        }
    }

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
    struct gpt_data *data = (struct gpt_data *)dev->data;

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

REGISTER_DEVICE_DRIVER(gpt, gpt_init)
