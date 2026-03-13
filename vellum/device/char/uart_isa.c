#include <stdlib.h>
#include <string.h>

#include <vellum/arch/io.h>

#include <vellum/device.h>
#include <vellum/interface/char.h>
#include <vellum/log.h>

#define MODULE_NAME "uart_isa"

struct uart_isa_data {
    uint16_t ioport;
};

static status_t write(struct device *dev, const char *buf, size_t len, size_t *result)
{
    struct uart_isa_data *data = (struct uart_isa_data *)dev->data;

    for (int i = 0; i < len; i++) {
        VlA_Out8(data->ioport, buf[i]);
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

static void uart_isa_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"uart_isa\"");
    }

    drv->name = "uart_isa";
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
    struct uart_isa_data *data = NULL;

    if (!rsrc || rsrc_cnt != 2 || rsrc[0].type != RT_IOPORT || rsrc[0].limit - rsrc[0].base != 8) {
        status = STATUS_INVALID_RESOURCE;
        goto has_error;
    }

    status = VlDev_Create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_GenerateName("ser", dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->ioport = rsrc[0].base;
    dev->data = data;

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
    struct uart_isa_data *data = (struct uart_isa_data *)dev->data;

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

REGISTER_DEVICE_DRIVER(uart_isa, uart_isa_init)
