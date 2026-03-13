#include <stdlib.h>
#include <string.h>

#include <vellum/arch/intrinsics/misc.h>
#include <vellum/arch/io.h>

#include <vellum/device.h>
#include <vellum/interface/char.h>
#include <vellum/log.h>

#define MODULE_NAME "ieee1284_isa"

struct ieee1284_isa_data {
    uint16_t io_base;
};

static status_t write(struct device *dev, const char *buf, size_t len, size_t *result)
{
    struct ieee1284_isa_data *data = (struct ieee1284_isa_data *)dev->data;

    for (int i = 0; i < len; i++) {
        while (!(VlA_In8(data->io_base + 1) & 0x80)) {
            VlA_Pause();
        }

        VlA_Out8(data->io_base, buf[i]);

        uint8_t cr = VlA_In8(data->io_base + 2);
        VlA_Out8(data->io_base + 2, cr | 0x1);
        for (volatile int j = 0; j < 2048; j++) {
        }
        VlA_Out8(data->io_base + 2, cr);
    }

    if (result) *result = len;

    return STATUS_SUCCESS;
}

static const struct char_interface charif = {
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

static void ieee1284_isa_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"ieee1284_isa\"");
    }

    drv->name = "ieee1284_isa";
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
    struct ieee1284_isa_data *data = NULL;

    if (!rsrc || rsrc_cnt != 2 || rsrc[0].type != RT_IOPORT || rsrc[0].limit - rsrc[0].base != 2) {
        status = STATUS_INVALID_RESOURCE;
        goto has_error;
    }

    status = VlDev_Create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_GenerateName("par", dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->io_base = rsrc[0].base;
    dev->data = data;

    VlA_Out8(data->io_base + 2, 0x0C);
    for (volatile int j = 0; j < 2048; j++) {
    }
    VlA_Out8(data->io_base + 2, 0x08);
    for (volatile int j = 0; j < 2048; j++) {
    }
    VlA_Out8(data->io_base + 2, 0x0C);

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
    struct ieee1284_isa_data *data = (struct ieee1284_isa_data *)dev->data;

    free(data);

    VlDev_Remove(dev);

    return STATUS_SUCCESS;
}

static status_t get_interface(struct device *dev, const char *name, const void **result)
{
    if (strcmp(name, "char") == 0) {
        if (result) *result = &charif;
        return STATUS_SUCCESS;
    }

    return STATUS_ENTRY_NOT_FOUND;
}

REGISTER_DEVICE_DRIVER(ieee1284_isa, ieee1284_isa_init)
