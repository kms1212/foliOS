#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/device.h>
#include <vellum/interface/block.h>
#include <vellum/interface/fdc.h>
#include <vellum/log.h>

#define MODULE_NAME "floppy"

struct floppy_data {
    int a;
};

static status_t get_block_size(struct device *dev, size_t *size)
{
    return STATUS_NOT_IMPLEMENTED;
}

static status_t read(struct device *dev, lba_t lba, void *buf, size_t count, size_t *result)
{
    return STATUS_NOT_IMPLEMENTED;
}

static status_t write(struct device *dev, lba_t lba, const void *buf, size_t count, size_t *result)
{
    return STATUS_NOT_IMPLEMENTED;
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

static void floppy_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"floppy\"");
    }

    drv->name = "floppy";
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
    struct device *fdcdev = NULL;
    const struct fdc_interface *fdcif = NULL;
    struct floppy_data *data = NULL;

    if (!rsrc || rsrc_cnt != 1 || rsrc[0].type != RT_BUS || rsrc[0].base != rsrc[0].limit) {
        status = STATUS_INVALID_RESOURCE;
        goto has_error;
    }

    fdcdev = parent;
    if (!fdcdev) {
        status = STATUS_INVALID_VALUE;
        goto has_error;
    }

    status = fdcdev->driver->get_interface(fdcdev, "fdc", (const void **)&fdcif);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_Create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_GenerateName("rd", dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    dev->data = data;

    LOG_DEBUG("initialization success\n");

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
    struct floppy_data *data = (struct floppy_data *)dev->data;

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

REGISTER_DEVICE_DRIVER(floppy, floppy_init)
