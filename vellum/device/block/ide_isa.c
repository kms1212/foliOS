#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/arch/intrinsics/misc.h>
#include <vellum/arch/io.h>

#include <vellum/plat/isr.h>

#include <vellum/device.h>
#include <vellum/interface/ide.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/status.h>

#define MODULE_NAME "ide_isa"

#define IDEREG_DATA     0
#define IDEREG_ERROR    1
#define IDEREG_FEATURES 1
#define IDEREG_COUNT    2
#define IDEREG_SECTOR   3
#define IDEREG_CYLLOW   4
#define IDEREG_CYLHIGH  5
#define IDEREG_DRVHEAD  6
#define IDEREG_STATUS   7
#define IDEREG_COMMAND  7

#define IDEREG_ALTSTAT 0
#define IDEREG_DEVCTRL 0
#define IDEREG_DRVADDR 1

struct ide_data {
    struct bus *ide_bus;
    uint16_t io_base0, io_base1;
    int irq_ch;
    struct isr_handler *isr;
};

static status_t bus_soft_reset(struct device *dev)
{
    struct ide_data *data = (struct ide_data *)dev->data;
    uint8_t sr;

    VlA_Out8(data->io_base1 + IDEREG_DEVCTRL, 0x04);
    VlA_In8(data->io_base1 + IDEREG_ALTSTAT);
    VlA_Out8(data->io_base1 + IDEREG_DEVCTRL, 0x00);
    for (int i = 0; i < 20; i++) {
        VlA_In8(data->io_base1 + IDEREG_ALTSTAT);
    }

    sr = VlA_In8(data->io_base1 + IDEREG_ALTSTAT);
    if (sr == 0xFF) return STATUS_HARDWARE_NOT_FOUND;

    do {
        VlA_Pause();
        sr = VlA_In8(data->io_base1 + IDEREG_ALTSTAT);
        if (sr & 0x01) return STATUS_HARDWARE_FAILED;
    } while (sr & 0x80);

    return STATUS_SUCCESS;
}

static status_t switch_device(struct device *dev, int slave)
{
    struct ide_data *data = (struct ide_data *)dev->data;
    uint8_t sr;

    if ((VlA_In8(data->io_base0 + IDEREG_DRVHEAD) & 0x10) != (slave ? 0x10 : 0x00)) {
        LOG_DEBUG("switching device...\n");
        VlA_Out8(data->io_base0 + IDEREG_DRVHEAD, 0xA0 | (slave ? 0x10 : 0x00));

    } else {
        sr = VlA_In8(data->io_base1 + IDEREG_ALTSTAT);
    }
    for (int i = 0; i < 15; i++) {
        sr = VlA_In8(data->io_base1 + IDEREG_ALTSTAT);
    }

    while (sr & 0x80) {
        VlA_Pause();
        sr = VlA_In8(data->io_base0 + IDEREG_STATUS);
        if (sr & 0x01) return STATUS_HARDWARE_FAILED;
    }

    return STATUS_SUCCESS;
}

static status_t send_command(struct device *dev, struct ata_command *cmd)
{
    struct ide_data *data = (struct ide_data *)dev->data;
    status_t status;
    uint8_t sr;

    /* switch master/slave device */
    status = switch_device(dev, cmd->drive_head & 0x10);
    if (!CHECK_SUCCESS(status)) return status;

    /* fill up register block */
    VlA_Out8(data->io_base0 + IDEREG_DRVHEAD, cmd->drive_head);

    if (cmd->extended) {
        VlA_Out8(data->io_base0 + IDEREG_FEATURES, (cmd->features >> 8) & 0xFF);
        VlA_Out8(data->io_base0 + IDEREG_COUNT, (cmd->count >> 8) & 0xFF);
        VlA_Out8(data->io_base0 + IDEREG_SECTOR, (cmd->sector >> 8) & 0xFF);
        VlA_Out8(data->io_base0 + IDEREG_CYLLOW, (cmd->cylinder_low >> 8) & 0xFF);
        VlA_Out8(data->io_base0 + IDEREG_CYLHIGH, (cmd->cylinder_high >> 8) & 0xFF);
    }

    VlA_Out8(data->io_base0 + IDEREG_FEATURES, cmd->features & 0xFF);
    VlA_Out8(data->io_base0 + IDEREG_COUNT, cmd->count & 0xFF);
    VlA_Out8(data->io_base0 + IDEREG_SECTOR, cmd->sector & 0xFF);
    VlA_Out8(data->io_base0 + IDEREG_CYLLOW, cmd->cylinder_low & 0xFF);
    VlA_Out8(data->io_base0 + IDEREG_CYLHIGH, cmd->cylinder_high & 0xFF);

    /* send command */
    VlA_Out8(data->io_base0 + IDEREG_COMMAND, cmd->command);

    /* wait until the command is finished */
    do {
        VlA_Pause();
        sr = VlA_In8(data->io_base0 + IDEREG_STATUS);
        if (sr & 0x01) return STATUS_HARDWARE_FAILED;
    } while (sr & 0x80);

    /* read result back */
    cmd->features = VlA_In8(data->io_base0 + IDEREG_ERROR);
    cmd->count = VlA_In8(data->io_base0 + IDEREG_COUNT);
    cmd->sector = VlA_In8(data->io_base0 + IDEREG_SECTOR);
    cmd->cylinder_low = VlA_In8(data->io_base0 + IDEREG_CYLLOW);
    cmd->cylinder_high = VlA_In8(data->io_base0 + IDEREG_CYLHIGH);

    /* check if the command had failed */
    if (sr & 0x21) return STATUS_HARDWARE_FAILED;

    return STATUS_SUCCESS;
}

static status_t send_command_pVlA_Input(
    struct device *dev,
    struct ata_command *cmd,
    void *buf,
    size_t size,
    size_t count,
    size_t *result
)
{
    struct ide_data *data = (struct ide_data *)dev->data;
    status_t status;
    uint8_t sr;
    size_t xfer_count = 0;

    /* send command */
    status = send_command(dev, cmd);
    if (!CHECK_SUCCESS(status)) return status;

    /* read out data */
    do {
        do {
            VlA_Pause();
            sr = VlA_In8(data->io_base0 + IDEREG_STATUS);
            if (sr & 0x21) goto end;
        } while (sr & 0x80);

        VlA_Ins16(data->io_base0 + IDEREG_DATA, buf + xfer_count * size, size / sizeof(uint16_t));
    } while (++xfer_count < count);

end:
    if (result) *result = xfer_count;

    /* check error again */
    sr = VlA_In8(data->io_base0 + IDEREG_STATUS);

    return (sr & 0x21) ? STATUS_HARDWARE_FAILED : STATUS_SUCCESS;
}

static status_t send_command_pVlA_Output(
    struct device *dev,
    struct ata_command *cmd,
    const void *buf,
    size_t size,
    size_t count,
    size_t *result
)
{
    struct ide_data *data = (struct ide_data *)dev->data;
    status_t status;
    uint8_t sr;
    size_t xfer_count = 0;

    /* send command */
    status = send_command(dev, cmd);
    if (!CHECK_SUCCESS(status)) return status;

    /* write out data */
    do {
        do {
            VlA_Pause();
            sr = VlA_In8(data->io_base0 + IDEREG_STATUS);
            if (sr & 0x01) goto end;
        } while (sr & 0x08);

        VlA_Outs16(data->io_base0 + IDEREG_DATA, buf + xfer_count * size, size / sizeof(uint16_t));
    } while (++xfer_count < count);

end:
    if (result) *result = xfer_count;

    /* check error again */
    sr = VlA_In8(data->io_base0 + IDEREG_STATUS);

    return (sr & 0x01) ? STATUS_HARDWARE_FAILED : STATUS_SUCCESS;
}

static status_t send_command_packet(
    struct device *dev, int slave, const uint8_t *packet, size_t packet_size, size_t data_block_size
)
{
    struct ide_data *data = (struct ide_data *)dev->data;
    status_t status;
    uint8_t sr;

    /* switch master/slave device */
    status = switch_device(dev, slave);
    if (!CHECK_SUCCESS(status)) return status;

    /* send PACKET command */
    LOG_DEBUG("sending PACKET command...\n");
    VlA_Out8(data->io_base0 + IDEREG_FEATURES, 0x00);
    VlA_Out8(data->io_base0 + IDEREG_CYLLOW, data_block_size & 0xFF);
    VlA_Out8(data->io_base0 + IDEREG_CYLHIGH, (data_block_size >> 8) & 0xFF);
    VlA_Out8(data->io_base0 + IDEREG_COMMAND, 0xA0);

    /* wait */
    for (int i = 0; i < 4; i++) {
        VlA_In8(data->io_base1 + IDEREG_ALTSTAT);
    }

    /* wait until the drive is ready to receive the packet */
    LOG_DEBUG("waiting for device to ready...\n");
    do {
        VlA_Pause();
        sr = VlA_In8(data->io_base0 + IDEREG_STATUS);
        if (sr & 0x01) return STATUS_HARDWARE_FAILED;
    } while ((sr & 0x80) || !(sr & 0x08));

    LOG_DEBUG("sending command...\n");
    VlA_Outs16(data->io_base0 + IDEREG_DATA, (const uint16_t *)packet, packet_size);

    /* wait until the command is finished */
    LOG_DEBUG("waiting until the command is finished...\n");
    do {
        VlA_Pause();
        sr = VlA_In8(data->io_base0 + IDEREG_STATUS);
        if (sr & 0x01) return STATUS_HARDWARE_FAILED;
    } while (sr & 0x80);

    return STATUS_SUCCESS;
}

static status_t send_command_packet_input(
    struct device *dev,
    int slave,
    const uint8_t *packet,
    size_t packet_size,
    void *buf,
    size_t size,
    size_t count,
    size_t *result
)
{
    struct ide_data *data = (struct ide_data *)dev->data;
    status_t status;
    uint8_t sr;
    size_t xfer_count = 0;
    size_t xfer_size;

    /* send command */
    status = send_command_packet(dev, slave, packet, packet_size, size);
    if (!CHECK_SUCCESS(status)) return status;

    /* read out data */
    do {
        LOG_DEBUG("waiting for DRQ...\n");
        do {
            VlA_Pause();
            sr = VlA_In8(data->io_base0 + IDEREG_STATUS);
            if (sr & 0x01) goto end;
        } while ((sr & 0x80) || !(sr & 0x08));

        xfer_size = ((size_t)VlA_In8(data->io_base0 + IDEREG_CYLHIGH) << 8) |
            VlA_In8(data->io_base0 + IDEREG_CYLLOW);
        LOG_DEBUG("transfer size=%zu\n", xfer_size);

        VlA_Ins16(
            data->io_base0 + IDEREG_DATA,
            buf + xfer_count * size,
            xfer_size / sizeof(uint16_t)
        );

        if (xfer_size < size) goto end;
    } while (++xfer_count < count);

end:
    if (result) *result = xfer_count;

    /* check error again */
    sr = VlA_In8(data->io_base0 + IDEREG_STATUS);

    return (sr & 0x01) ? STATUS_HARDWARE_FAILED : STATUS_SUCCESS;
}

static status_t send_command_packet_output(
    struct device *dev,
    int slave,
    const uint8_t *packet,
    size_t packet_size,
    const void *buf,
    size_t size,
    size_t count,
    size_t *result
)
{
    return STATUS_NOT_SUPPORTED;
}

const struct ide_interface ideif = {
    .send_command = send_command,
    .send_command_pVlA_Input = send_command_pVlA_Input,
    .send_command_pVlA_Output = send_command_pVlA_Output,
    .send_command_packet = send_command_packet,
    .send_command_packet_input = send_command_packet_input,
    .send_command_packet_output = send_command_packet_output,
};

static status_t detect_device(struct device *dev, int slave, int *atapi)
{
    struct ide_data *data = (struct ide_data *)dev->data;
    status_t status;
    uint16_t idbuf[256];
    struct ata_command cmd;
    uint8_t sr, cyl_low, cyl_high;

    LOG_DEBUG("soft-resetting bus...\n");
    status = bus_soft_reset(dev);
    if (!CHECK_SUCCESS(status)) return status;

    LOG_DEBUG("switching device...\n");
    VlA_Out8(data->io_base0 + IDEREG_DRVHEAD, 0xA0 | (slave ? 0x10 : 0x00));
    VlA_In8(data->io_base1 + IDEREG_ALTSTAT);
    VlA_In8(data->io_base1 + IDEREG_ALTSTAT);
    VlA_In8(data->io_base1 + IDEREG_ALTSTAT);
    sr = VlA_In8(data->io_base1 + IDEREG_ALTSTAT);

    cyl_low = VlA_In8(data->io_base0 + IDEREG_CYLLOW);
    cyl_high = VlA_In8(data->io_base0 + IDEREG_CYLHIGH);

    if (!sr && cyl_low != 0x14 && cyl_high != 0xEB) return STATUS_HARDWARE_NOT_FOUND;

    if (cyl_low == 0x00 && cyl_high == 0x00) {
        LOG_DEBUG("sending IDENTIFY DEVICE command...\n");
        cmd = (struct ata_command){
            .extended = 0,
            .command = 0xEC, /* IDENTIFY DEVICE */
            .features = 0,
            .count = 0,
            .sector = 0,
            .cylinder_low = 0,
            .cylinder_high = 0,
            .drive_head = 0xA0 | (slave ? 0x10 : 0x00),
        };

        status = send_command_pVlA_Input(dev, &cmd, idbuf, sizeof(idbuf), 1, NULL);
        if (!CHECK_SUCCESS(status)) return status;

        if (atapi) *atapi = 0;
        return STATUS_SUCCESS;
    } else if (cyl_low == 0x14 && cyl_high == 0xEB) {
        LOG_DEBUG("sending IDENTIFY PACKET DEVICE command...\n");
        cmd = (struct ata_command){
            .extended = 0,
            .command = 0xA1, /* IDENTIFY PACKET DEVICE */
            .features = 0,
            .count = 0,
            .sector = 0,
            .cylinder_low = 0,
            .cylinder_high = 0,
            .drive_head = 0xA0 | (slave ? 0x10 : 0x00),
        };

        status = send_command_pVlA_Input(dev, &cmd, idbuf, sizeof(idbuf), 1, NULL);
        if (!CHECK_SUCCESS(status)) return status;

        if (atapi) *atapi = 1;
        return STATUS_SUCCESS;
    } else {
        return STATUS_NOT_SUPPORTED;
    }
}

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

static void ide_isa_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"ide_isa\"");
    }

    drv->name = "ide_isa";
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
    struct ide_data *data = NULL;
    int master_atapi, master_present, slave_atapi, slave_present;
    struct device *drvdev = NULL;
    struct device_driver *atadrv = NULL;
    struct device_driver *atapidrv = NULL;

    if (!rsrc || rsrc_cnt != 3 || rsrc[0].type != RT_IOPORT || rsrc[0].limit - rsrc[0].base != 7 ||
        rsrc[1].type != RT_IOPORT || rsrc[1].limit - rsrc[1].base != 1 || rsrc[2].type != RT_IRQ ||
        rsrc[2].base != rsrc[2].limit) {
        return STATUS_INVALID_RESOURCE;
    }

    status = VlDev_Create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_GenerateName("idec", dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->io_base0 = rsrc[0].base;
    data->io_base1 = rsrc[1].base;
    data->irq_ch = (int)rsrc[2].base;
    data->isr = NULL;
    dev->data = data;

    status = VlIntP_Mask(data->irq_ch);
    if (!CHECK_SUCCESS(status)) goto has_error;

    LOG_DEBUG("registering interrupt service routine...\n");
    status = VlIntP_AddInterruptHandler(data->irq_ch, dev, isr, &data->isr);
    if (!CHECK_SUCCESS(status)) goto has_error;

    LOG_DEBUG("detecting master device...\n");
    status = detect_device(dev, 0, &master_atapi);
    master_present = CHECK_SUCCESS(status);
    if (master_present) {
        LOG_DEBUG("master device detected as %s\n", master_atapi ? "atapi" : "ata");
    } else {
        LOG_DEBUG("master device not detected\n");
    }

    LOG_DEBUG("detecting slave device...\n");
    status = detect_device(dev, 1, &slave_atapi);
    slave_present = CHECK_SUCCESS(status);
    if (slave_present) {
        LOG_DEBUG("slave device detected as %s\n", slave_atapi ? "atapi" : "ata");
    } else {
        LOG_DEBUG("slave device not detected\n");
    }

    if ((master_present && !master_atapi) || (slave_present && !slave_atapi)) {
        status = VlDev_FindDriver("ata", &atadrv);
        if (!CHECK_SUCCESS(status)) goto has_error;
    }

    if ((master_present && master_atapi) || (slave_present && slave_atapi)) {
        status = VlDev_FindDriver("atapi", &atapidrv);
        if (!CHECK_SUCCESS(status)) goto has_error;
    }

    if (master_present) {
        LOG_DEBUG("initializing master device...\n");

        struct resource res[] = {
            {
                .type = RT_BUS,
                .base = 0,
                .limit = 0,
                .flags = 0,
            },
        };

        if (master_atapi) {
            atapidrv->probe(&drvdev, atapidrv, dev, res, ARRAY_SIZE(res));
        } else {
            atadrv->probe(&drvdev, atadrv, dev, res, ARRAY_SIZE(res));
        }
    }

    if (slave_present) {
        LOG_DEBUG("initializing slave device...\n");

        struct resource res[] = {
            {
                .type = RT_BUS,
                .base = 1,
                .limit = 1,
                .flags = 0,
            },
        };

        if (slave_atapi) {
            atapidrv->probe(&drvdev, atapidrv, dev, res, ARRAY_SIZE(res));
        } else {
            atadrv->probe(&drvdev, atadrv, dev, res, ARRAY_SIZE(res));
        }
    }

    status = VlIntP_Unmask(data->irq_ch);
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (devout) *devout = dev;

    LOG_DEBUG("initialization success\n");

    return STATUS_SUCCESS;

has_error:
    VlIntP_Unmask((int)rsrc[2].base);

    if (data && data->isr) {
        VlIntP_RemoveHandler(data->isr);
    }

    if (dev) {
        for (struct device *diskdev = dev->first_child; diskdev; diskdev = diskdev->sibling) {
            diskdev->driver->remove(diskdev);
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
    struct ide_data *data = (struct ide_data *)dev->data;

    VlIntP_Unmask(data->irq_ch);

    VlIntP_RemoveHandler(data->isr);

    if (dev) {
        for (struct device *diskdev = dev->first_child; diskdev; diskdev = diskdev->sibling) {
            diskdev->driver->remove(diskdev);
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
    if (strcmp(name, "ide") == 0) {
        if (result) *result = &ideif;
        return STATUS_SUCCESS;
    }

    return STATUS_ENTRY_NOT_FOUND;
}

REGISTER_DEVICE_DRIVER(ide_isa, ide_isa_init)
