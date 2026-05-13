#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/arch/intrinsics/io.h>

#include <vellum/device.h>
#include <vellum/interface/char.h>
#include <vellum/plat/panic.h>
#include <vellum/resource.h>
#include <vellum/status.h>

struct debugout_data {
    uint16_t ioport;
};

static VlStatus write(struct device *dev, const char *buf, size_t len, size_t *result)
{
    struct debugout_data *data = (struct debugout_data *)dev->data;

    for (int i = 0; buf[i] && i < len; i++) {
        VlA_Out8(data->ioport, buf[i]);
    }

    if (result) *result = len;

    return STATUS_SUCCESS;
}

static const struct char_interface charif = {
    .write = write,
};

static VlStatus probe(
    struct device **devout,
    struct device_driver *drv,
    struct device *parent,
    struct resource *rsrc,
    int rsrc_cnt
);
static VlStatus remove(struct device *dev);
static VlStatus get_interface(struct device *dev, const char *name, const void **result);

static void debugout_init(void)
{
    VlStatus status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"debugout\"");
    }

    drv->name = "debugout";
    drv->probe = probe;
    drv->remove = remove;
    drv->get_interface = get_interface;
}

static VlStatus probe(
    struct device **devout,
    struct device_driver *drv,
    struct device *parent,
    struct resource *rsrc,
    int rsrc_cnt
)
{
    VlStatus status;
    struct device *dev = NULL;
    struct debugout_data *data = NULL;

    if (!rsrc || rsrc_cnt != 1 || rsrc[0].type != RT_IOPORT || rsrc[0].base != rsrc[0].limit) {
        status = STATUS_INVALID_RESOURCE;
        goto has_error;
    }

    status = VlDev_Create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_GenerateName("dbg", dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->ioport = rsrc[0].base;
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

static VlStatus remove(struct device *dev)
{
    struct debugout_data *data = (struct debugout_data *)dev->data;

    free(data);

    VlDev_Remove(dev);

    return STATUS_SUCCESS;
}

static VlStatus get_interface(struct device *dev, const char *name, const void **result)
{
    if (strcmp(name, "char") == 0) {
        if (result) *result = &charif;
        return STATUS_SUCCESS;
    }

    return STATUS_ENTRY_NOT_FOUND;
}

REGISTER_DEVICE_DRIVER(debugout, debugout_init)
