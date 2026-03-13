#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/arch/intrinsics/misc.h>
#include <vellum/arch/io.h>

#include <vellum/plat/isr.h>
#include <vellum/plat/time.h>

#include <vellum/device.h>
#include <vellum/interface/fdc.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/status.h>

#define MODULE_NAME "fdc_isa"

#define FDCREG_SRA  0x0
#define FDCREG_SRB  0x1
#define FDCREG_DOR  0x2
#define FDCREG_TDR  0x3
#define FDCREG_MSR  0x4
#define FDCREG_DRSR 0x4
#define FDCREG_FIFO 0x5
#define FDCREG_DIR  0x7
#define FDCREG_CCR  0x7

struct fdc_data {
    uint16_t io_base;
    int irq_ch, dma_ch;
    struct isr_handler *isr;

    uint8_t dor;
    volatile uint8_t irq_received;

    struct {
        uint8_t recalib, state, track;
    } drive_state[4];
};

static status_t wait_irq(struct device *dev, unsigned int timeout)
{
    struct fdc_data *data = (struct fdc_data *)dev->data;

    uint64_t tick_start = get_global_tick();

    if (data->irq_received) {
        data->irq_received = 0;
    }
    while (!data->irq_received) {
        if (get_global_tick() - tick_start > timeout) return 1;
    }
    data->irq_received = 0;

    return STATUS_SUCCESS;
}

static status_t send_command(struct device *dev, struct fdc_command *cmd)
{
    struct fdc_data *data = (struct fdc_data *)dev->data;

    status_t status;
    uint8_t reg;

    for (int i = 0; i < cmd->send_size + 1; i++) {
        while (!(VlA_In8(data->io_base + FDCREG_MSR) & 0x80)) {
            VlA_Pause();
        }
        reg = VlA_In8(data->io_base + FDCREG_MSR);
        if (reg & 0x40) {
            return 1;
        }

        if (i > 0) {
            VlA_Out8(data->io_base + FDCREG_FIFO, cmd->data[i - 1]);
        } else {
            VlA_Out8(data->io_base + FDCREG_FIFO, cmd->data[0]);
        }
    }

    if (cmd->wait_irq) {
        status = wait_irq(dev, 1);
        if (!CHECK_SUCCESS(status)) return status;
    }

    for (int i = 0; i < cmd->recv_size; i++) {
        while (!(VlA_In8(data->io_base + FDCREG_MSR) & 0x80)) {
            VlA_Pause();
        }
        reg = VlA_In8(data->io_base + FDCREG_MSR);
        if (reg & 0x40) {
            return 1;
        }

        cmd->data[i] = VlA_In8(data->io_base + FDCREG_FIFO);
    }

    reg = VlA_In8(data->io_base + FDCREG_MSR);
    if (reg & 0x40) {
        return 1;
    }

    return STATUS_SUCCESS;
}

static status_t fdc_reset(struct device *dev)
{
    struct fdc_data *data = (struct fdc_data *)dev->data;

    for (int i = 0; i < 4; i++) {
        data->drive_state[i].recalib = 0;
        data->drive_state[i].state = 0;
        data->drive_state[i].track = 0;
    }

    struct fdc_command cmd;
    cmd.send_size = 3;
    cmd.recv_size = 0;
    cmd.wait_irq = 0;
    cmd.command = 0x13;
    cmd.data[0] = 0x00;
    cmd.data[0] = 0x20;
    cmd.data[0] = 0x00;
    send_command(dev, &cmd);

    return STATUS_SUCCESS;
}

static status_t detect_device(struct device *dev, int slave, int *atapi)
{
    return STATUS_SUCCESS;
}

static status_t reset(struct device *dev, int drive)
{
    struct fdc_data *data = (struct fdc_data *)dev->data;

    data->drive_state[drive].recalib = 0;
    data->drive_state[drive].state = 0;
    data->drive_state[drive].track = 0;

    return STATUS_SUCCESS;
}

static const struct fdc_interface fdcif = {
    .reset = reset,
};

static void isr(void *data, struct VlA_InterruptFrame *frame, struct trap_regs *regs, int num) {}

static status_t probe(
    struct device **devout,
    struct device_driver *drv,
    struct device *parent,
    struct resource *rsrc,
    int rsrc_cnt
);
static status_t remove(struct device *dev);
static status_t get_interface(struct device *dev, const char *name, const void **result);

static void fdc_isa_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"fdc_isa\"");
    }

    drv->name = "fdc_isa";
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
    struct fdc_data *data = NULL;
    struct device *fpdev = NULL;
    struct device_driver *fpdrv = NULL;

    if (!rsrc || rsrc_cnt != 3 || rsrc[0].type != RT_IOPORT || rsrc[0].limit - rsrc[0].base != 7 ||
        rsrc[1].type != RT_IRQ || rsrc[1].base != rsrc[1].limit || rsrc[2].type != RT_DMA ||
        rsrc[2].base != rsrc[2].limit) {
        return STATUS_INVALID_RESOURCE;
    }

    status = VlDev_Create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_GenerateName("fdc", dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->io_base = rsrc[0].base;
    data->irq_ch = (int)rsrc[1].base;
    data->dma_ch = (int)rsrc[2].base;
    data->dor = 0;
    data->irq_received = 0;
    data->isr = NULL;
    for (int i = 0; i < 4; i++) {
        data->drive_state[i].recalib = 0;
        data->drive_state[i].state = 0;
        data->drive_state[i].track = 0;
    }
    dev->data = data;

    status = VlIntP_Mask(data->irq_ch);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlIntP_AddInterruptHandler(data->irq_ch, dev, isr, &data->isr);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = fdc_reset(dev);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_FindDriver("floppy", &fpdrv);
    if (!CHECK_SUCCESS(status)) goto has_error;

    for (int i = 0; i < 4; i++) {
        struct resource res[] = {
            {
                .type = RT_BUS,
                .base = i,
                .limit = i,
                .flags = 0,
            },
        };

        status = fpdrv->probe(&fpdev, fpdrv, dev, res, ARRAY_SIZE(res));
        if (!CHECK_SUCCESS(status)) goto has_error;
    }

    status = VlIntP_Unmask(data->irq_ch);
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (devout) *devout = dev;

    LOG_DEBUG("initialization success\n");

    return STATUS_SUCCESS;

has_error:
    VlIntP_Unmask((int)rsrc[1].base);

    if (data && data->isr) {
        VlIntP_RemoveHandler(data->isr);
    }

    if (dev) {
        for (struct device *fddev = dev->first_child; fddev; fddev = fddev->sibling) {
            fddev->driver->remove(fddev);
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
    struct fdc_data *data = (struct fdc_data *)dev->data;

    VlIntP_Unmask(data->irq_ch);

    VlIntP_RemoveHandler(data->isr);

    if (dev) {
        for (struct device *fddev = dev->first_child; fddev; fddev = fddev->sibling) {
            fddev->driver->remove(fddev);
        }
    }

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
    if (strcmp(name, "fdc") == 0) {
        if (result) *result = &fdcif;
        return STATUS_SUCCESS;
    }

    return STATUS_ENTRY_NOT_FOUND;
}

REGISTER_DEVICE_DRIVER(fdc_isa, fdc_isa_init)
