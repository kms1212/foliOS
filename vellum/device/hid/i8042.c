#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/arch/intrinsics/misc.h>
#include <vellum/arch/io.h>

#include <vellum/plat/isr.h>
#include <vellum/plat/time.h>

#include <vellum/device.h>
#include <vellum/hid.h>
#include <vellum/interface/ps2.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/status.h>

#define MODULE_NAME "i8042"

struct i8042_data {
    uint16_t io_data, io_ctrl;
    int irq_port0, irq_port1;
    struct resource *res[2];
};

struct key_event {
    uint16_t keycode;
    uint16_t flags;
};

static status_t wait_for_status_register(
    struct device *dev, uint8_t value, uint8_t mask, unsigned int timeout
)
{
    struct i8042_data *data = (struct i8042_data *)dev->data;

    uint64_t tick_start = get_global_tick();

    while ((VlA_In8(data->io_ctrl) & mask) != value) {
        if (get_global_tick() - tick_start > timeout) return STATUS_IO_TIMEOUT;
        VlA_Pause();
    }

    return STATUS_SUCCESS;
}

static status_t read_ccb(struct device *dev, uint8_t *value)
{
    struct i8042_data *data = (struct i8042_data *)dev->data;
    status_t status;
    uint8_t ccb;

    status = wait_for_status_register(dev, 0x00, 0x02, 2);
    if (!CHECK_SUCCESS(status)) return status;
    VlA_Out8(data->io_ctrl, 0x20);

    status = wait_for_status_register(dev, 0x01, 0x01, 2);
    if (!CHECK_SUCCESS(status)) return status;
    ccb = VlA_In8(data->io_data);

    if (value) *value = ccb;

    return STATUS_SUCCESS;
}

static status_t write_ccb(struct device *dev, uint8_t value)
{
    struct i8042_data *data = (struct i8042_data *)dev->data;
    status_t status;

    status = wait_for_status_register(dev, 0x00, 0x02, 2);
    if (!CHECK_SUCCESS(status)) return status;
    VlA_Out8(data->io_ctrl, 0x60);

    status = wait_for_status_register(dev, 0x00, 0x02, 2);
    if (!CHECK_SUCCESS(status)) return status;
    VlA_Out8(data->io_data, value);

    return STATUS_SUCCESS;
}

static status_t enable_port(struct device *dev, int port)
{
    struct i8042_data *data = (struct i8042_data *)dev->data;
    status_t status;
    uint8_t prev_ccb;

    status = wait_for_status_register(dev, 0x00, 0x02, 2);
    if (!CHECK_SUCCESS(status)) return status;
    VlA_Out8(data->io_ctrl, port ? 0xA8 : 0xAE);

    status = read_ccb(dev, &prev_ccb);
    if (!CHECK_SUCCESS(status)) return status;

    status = write_ccb(dev, prev_ccb | (port ? 0x02 : 0x01));
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static status_t disable_port(struct device *dev, int port)
{
    struct i8042_data *data = (struct i8042_data *)dev->data;
    status_t status;
    uint8_t prev_ccb;

    status = wait_for_status_register(dev, 0x00, 0x02, 2);
    if (!CHECK_SUCCESS(status)) return status;
    VlA_Out8(data->io_ctrl, port ? 0xA7 : 0xAD);

    status = read_ccb(dev, &prev_ccb);
    if (!CHECK_SUCCESS(status)) return status;

    status = write_ccb(dev, prev_ccb & ~(port ? 0x02 : 0x01));
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static status_t test_port(struct device *dev, int port)
{
    struct i8042_data *data = (struct i8042_data *)dev->data;
    status_t status;

    status = wait_for_status_register(dev, 0x00, 0x02, 2);
    if (!CHECK_SUCCESS(status)) return status;
    VlA_Out8(data->io_ctrl, port ? 0xA9 : 0xAB);

    status = wait_for_status_register(dev, 0x01, 0x01, 2);
    if (!CHECK_SUCCESS(status)) return status;

    LOG_DEBUG("testing port #%d\n", port);

    return VlA_In8(data->io_data) ? STATUS_HARDWARE_NOT_FOUND : STATUS_SUCCESS;
}

static status_t send_data(struct device *dev, int port, const uint8_t *buf, int len)
{
    struct i8042_data *data = (struct i8042_data *)dev->data;
    status_t status;

    for (int i = 0; i < len; i++) {
        if (port) {
            status = wait_for_status_register(dev, 0x00, 0x02, 2);
            if (!CHECK_SUCCESS(status)) return status;
            VlA_Out8(data->io_ctrl, 0xD4);
        }

        status = wait_for_status_register(dev, 0x00, 0x02, 2);
        if (!CHECK_SUCCESS(status)) return status;

        VlA_Out8(data->io_data, buf[i]);
    }

    return STATUS_SUCCESS;
}

static status_t recv_data(struct device *dev, int port, uint8_t *buf, int len)
{
    struct i8042_data *data = (struct i8042_data *)dev->data;
    status_t status;

    for (int i = 0; i < len; i++) {
        status = wait_for_status_register(dev, 0x01, 0x01, 2);
        if (!CHECK_SUCCESS(status)) return status;

        buf[i] = VlA_In8(data->io_data);
    }

    return STATUS_SUCCESS;
}

static status_t irq_get_byte(struct device *dev, uint8_t *result)
{
    struct i8042_data *data = (struct i8042_data *)dev->data;

    uint8_t byte = VlA_In8(data->io_data);

    if (result) *result = byte;

    return STATUS_SUCCESS;
}

static const struct ps2_interface ps2if = {
    .enable_port = enable_port,
    .disable_port = disable_port,
    .test_port = test_port,
    .send_data = send_data,
    .recv_data = recv_data,
    .irq_get_byte = irq_get_byte,
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

static void i8042_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"i8042\"");
    }

    drv->name = "i8042";
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
    struct i8042_data *data = NULL;
    struct device *idev = NULL;
    struct device_driver *kbdrv = NULL;
    struct device_driver *msdrv = NULL;
    uint8_t prev_ccb;

    if (!rsrc || rsrc_cnt != 4 || rsrc[0].type != RT_IOPORT || rsrc[0].base != rsrc[0].limit ||
        rsrc[1].type != RT_IOPORT || rsrc[1].base != rsrc[1].limit || rsrc[2].type != RT_IRQ ||
        rsrc[2].base != rsrc[2].limit || rsrc[3].type != RT_IRQ || rsrc[3].base != rsrc[3].limit) {
        status = STATUS_INVALID_RESOURCE;
        goto has_error;
    }

    status = VlDev_Create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_GenerateName("ps2c", dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->io_data = rsrc[0].base;
    data->io_ctrl = rsrc[1].base;
    data->irq_port0 = (int)rsrc[2].base;
    data->irq_port1 = (int)rsrc[3].base;
    dev->data = data;

    LOG_DEBUG("disabling all connected peripherals...\n");
    /* disable all devices */
    status = disable_port(dev, 0);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = disable_port(dev, 1);
    if (!CHECK_SUCCESS(status)) goto has_error;

    LOG_DEBUG("flushing output buffer...\n");
    /* flush the output buffer*/
    VlA_In8(data->io_data);

    LOG_DEBUG("disabling translation & IRQ...\n");
    /* disable translation & IRQ */
    status = read_ccb(dev, &prev_ccb);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = write_ccb(dev, prev_ccb & ~0x43);
    if (!CHECK_SUCCESS(status)) goto has_error;

    LOG_DEBUG("running controller self-test...\n");
    /* perform controller self-test */
    VlA_Out8(data->io_ctrl, 0xAA);

    status = wait_for_status_register(dev, 0x01, 0x01, 2);
    if (!CHECK_SUCCESS(status)) goto has_error;
    if (VlA_In8(data->io_data) != 0x55) return STATUS_HARDWARE_FAILED;

    LOG_DEBUG("disabling translation & IRQ again...\n");
    /* disable translation & IRQ (again) */
    status = read_ccb(dev, &prev_ccb);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = write_ccb(dev, prev_ccb & ~0x43);
    if (!CHECK_SUCCESS(status)) goto has_error;

    /* initialize child devices */
    status = VlDev_FindDriver("ps2_keyboard", &kbdrv);
    if (!CHECK_SUCCESS(status)) {
        kbdrv = NULL;
    }

    status = VlDev_FindDriver("ps2_mouse", &msdrv);
    if (!CHECK_SUCCESS(status)) {
        msdrv = NULL;
    }

    if (msdrv) {
        LOG_DEBUG("initializing second port...\n");

        struct resource res[] = {
            {
                .type = RT_BUS,
                .base = 1,
                .limit = 1,
                .flags = 0,
            },
            {
                .type = RT_IRQ,
                .base = data->irq_port1,
                .limit = data->irq_port1,
                .flags = 0,
            },
        };

        /* status = */ msdrv->probe(&idev, msdrv, dev, res, ARRAY_SIZE(res));
        // ignore status
    }

    if (kbdrv) {
        LOG_DEBUG("initializing first port...\n");

        struct resource res[] = {
            {
                .type = RT_BUS,
                .base = 0,
                .limit = 0,
                .flags = 0,
            },
            {
                .type = RT_IRQ,
                .base = data->irq_port0,
                .limit = data->irq_port0,
                .flags = 0,
            },
        };

        /* status = */ kbdrv->probe(&idev, kbdrv, dev, res, ARRAY_SIZE(res));
        // ignore status
    }

    LOG_DEBUG("initialization success\n");

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
    struct i8042_data *data = (struct i8042_data *)dev->data;

    for (struct device *partdev = dev->first_child; partdev; partdev = partdev->sibling) {
        partdev->driver->remove(partdev);
    }

    /* enable translation & IRQ */
    VlA_Out8(data->io_ctrl, 0x60);
    VlA_Out8(data->io_data, 0x67);

    free(data);

    VlDev_Remove(dev);

    return STATUS_SUCCESS;
}

static status_t get_interface(struct device *dev, const char *name, const void **result)
{
    if (strcmp(name, "ps2") == 0) {
        if (result) *result = &ps2if;
        return STATUS_SUCCESS;
    }

    return STATUS_ENTRY_NOT_FOUND;
}

REGISTER_DEVICE_DRIVER(i8042, i8042_init)
