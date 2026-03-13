#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/device.h>
#include <vellum/interface/block.h>
#include <vellum/interface/ide.h>
#include <vellum/log.h>
#include <vellum/status.h>

#include "ata.h"
#include "atapi.h"

#define MODULE_NAME "atapi"

struct atapi_data {
    struct device *idedev;
    const struct ide_interface *ideif;
    int slave;
};

static status_t get_block_size(struct device *dev, size_t *size)
{
    if (size) *size = 2048;

    return STATUS_SUCCESS;
}

static status_t read(struct device *dev, lba_t lba, void *buf, size_t count, size_t *result)
{
    status_t status;
    uint8_t command[12];
    struct atapi_data *data = (struct atapi_data *)dev->data;
    size_t xfer_count;

    command[0] = 0xA8;
    command[1] = 0x00;
    command[2] = (lba >> 24) & 0xFF;
    command[3] = (lba >> 16) & 0xFF;
    command[4] = (lba >> 8) & 0xFF;
    command[5] = lba & 0xFF;
    command[6] = (count >> 24) & 0xFF;
    command[7] = (count >> 16) & 0xFF;
    command[8] = (count >> 8) & 0xFF;
    command[9] = count & 0xFF;
    command[10] = 0;
    command[11] = 0;

    status = data->ideif->send_command_packet_input(
        data->idedev,
        data->slave,
        command,
        sizeof(command),
        buf,
        2048,
        count,
        &xfer_count
    );
    if (!CHECK_SUCCESS(status)) return status;

    if (result) *result = xfer_count;

    return STATUS_SUCCESS;
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

static void atapi_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"atapi\"");
    }

    drv->name = "atapi";
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
    struct device *idedev = NULL;
    const struct ide_interface *ideif = NULL;
    struct atapi_data *data = NULL;
    struct atapi_device_ident atapi_ident;

    if (!rsrc || rsrc_cnt != 1 || rsrc[0].type != RT_BUS || rsrc[0].base != rsrc[0].limit ||
        rsrc[0].base > 1) {
        status = STATUS_INVALID_RESOURCE;
        goto has_error;
    }

    idedev = parent;
    if (!idedev) {
        status = STATUS_INVALID_VALUE;
        goto has_error;
    }

    status = idedev->driver->get_interface(idedev, "ide", (const void **)&ideif);
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

    data->idedev = idedev;
    data->ideif = ideif;
    data->slave = (int)rsrc[0].base;
    dev->data = data;

    LOG_DEBUG("identifying device...\n");
    struct ata_command cmd = {
        .extended = 0,
        .command = 0xA1, /* IDENTIFY PACKET DEVICE */
        .features = 0,
        .count = 0,
        .sector = 0,
        .cylinder_low = 0,
        .cylinder_high = 0,
        .drive_head = 0xA0 | (data->slave ? 0x10 : 0x00),
    };

    status =
        ideif->send_command_pVlA_Input(idedev, &cmd, &atapi_ident, sizeof(atapi_ident), 1, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    if (devout) *devout = dev;

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
    struct atapi_data *data = (struct atapi_data *)dev->data;

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

REGISTER_DEVICE_DRIVER(atapi, atapi_init)
