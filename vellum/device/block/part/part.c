#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <endian.h>

#include <vellum/device.h>
#include <vellum/interface/block.h>
#include <vellum/status.h>

struct part_data {
    struct device *blkdev;
    const struct block_interface *blkif;
    lba_t part_base, part_limit;
};

static status_t get_block_size(struct device *dev, size_t *size)
{
    struct part_data *data = (struct part_data *)dev->data;

    return data->blkif->get_block_size(data->blkdev, size);
}

static status_t read(struct device *dev, lba_t lba, void *buf, size_t count, size_t *result)
{
    struct part_data *data = (struct part_data *)dev->data;

    if (lba > data->part_limit) {
        return STATUS_INVALID_VALUE;
    }

    if (data->part_base + lba + count > data->part_limit) {
        count -= data->part_base + lba + count - data->part_limit;
    }

    if (count < 1) return STATUS_SUCCESS;

    return data->blkif->read(data->blkdev, data->part_base + lba, buf, count, result);
}

static status_t write(struct device *dev, lba_t lba, const void *buf, size_t count, size_t *result)
{
    struct part_data *data = (struct part_data *)dev->data;

    if (lba > data->part_limit) {
        return STATUS_INVALID_VALUE;
    }

    if (data->part_base + lba + count > data->part_limit) {
        count -= data->part_base + lba + count - data->part_limit;
    }

    if (count < 1) return STATUS_SUCCESS;

    return data->blkif->write(data->blkdev, data->part_base + lba, buf, count, result);
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

static void part_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"part\"");
    }

    drv->name = "part";
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
    struct part_data *data = NULL;

    if (!rsrc || rsrc_cnt != 1 || rsrc[0].type != RT_LBA || rsrc[0].base > rsrc[0].limit) {
        status = STATUS_INVALID_RESOURCE;
        goto has_error;
    }

    blkdev = parent;
    if (!blkdev) {
        status = STATUS_INVALID_VALUE;
        goto has_error;
    }

    status = blkdev->driver->get_interface(blkdev, "block", (const void **)&blkif);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_Create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    snprintf(dev_name_base, sizeof(dev_name_base), "%.62sp", parent->name);

    status = VlDev_GenerateName(dev_name_base, dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->blkdev = blkdev;
    data->blkif = blkif;
    data->part_base = le64toh(rsrc[0].base);
    data->part_limit = le64toh(rsrc[0].limit);
    dev->data = data;

    if (devout) *devout = dev;

    return STATUS_SUCCESS;

has_error:
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
    struct part_data *data = (struct part_data *)dev->data;

    if (data) {
        free(data);
    }

    if (dev) {
        VlDev_Remove(dev);
    }

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

REGISTER_DEVICE_DRIVER(part, part_init)
