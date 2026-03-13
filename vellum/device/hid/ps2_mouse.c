#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/arch/intrinsics/misc.h>
#include <vellum/arch/io.h>

#include <vellum/plat/isr.h>
#include <vellum/plat/time.h>

#include <vellum/device.h>
#include <vellum/hid.h>
#include <vellum/interface/char.h>
#include <vellum/interface/hid.h>
#include <vellum/interface/ps2.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/status.h>

#define MODULE_NAME "ps2mse"

enum sequence_state {
    SS_DEFAULT = 0,
    SS_XMOVEMENT,
    SS_YMOVEMENT,
    SS_ZMOVEMENT,
};

struct ps2_mouse_data {
    struct device *ps2dev;
    const struct ps2_interface *ps2if;

    int slave, irq_num;
    struct isr_handler *isr;
    uint8_t device_type;

    volatile unsigned int seqbuf_start, seqbuf_end;
    volatile uint8_t seqbuf[64];
    enum sequence_state seq_state;
    uint8_t prev_button_state, byte0;
};

static status_t wait_event(struct device *dev)
{
    struct ps2_mouse_data *data = (struct ps2_mouse_data *)dev->data;

    while (data->seqbuf_start == data->seqbuf_end) {
        VlA_Pause();
    }

    return STATUS_SUCCESS;
}

static status_t poll_event(struct device *dev, uint16_t *key, uint16_t *flags)
{
    struct ps2_mouse_data *data = (struct ps2_mouse_data *)dev->data;
    status_t status;
    int advance = 0;
    uint16_t ret_key = 0, ret_flags = 0;
    uint8_t byte;

    if (data->seqbuf_start == data->seqbuf_end) return STATUS_NO_EVENT;

    status = VlIntP_Mask(data->irq_num);
    if (!CHECK_SUCCESS(status)) goto has_error;

    byte = data->seqbuf[data->seqbuf_start];
    status = STATUS_SUCCESS;
    switch (data->seq_state) {
    case SS_DEFAULT:
        if ((byte & 0x08) && !(byte & 0xC0)) {
            if ((byte & 0x04) != (data->prev_button_state & 0x04)) {
                ret_key = KEY_MOUSEBTNM;
                ret_flags = (byte & 0x04) ? 0 : KEY_FLAG_BREAK;
                data->prev_button_state = (data->prev_button_state & ~0x04) | (byte & 0x04);
            } else if ((byte & 0x02) != (data->prev_button_state & 0x02)) {
                ret_key = KEY_MOUSEBTNR;
                ret_flags = (byte & 0x02) ? 0 : KEY_FLAG_BREAK;
                data->prev_button_state = (data->prev_button_state & ~0x02) | (byte & 0x02);
            } else if ((byte & 0x01) != (data->prev_button_state & 0x01)) {
                ret_key = KEY_MOUSEBTNL;
                ret_flags = (byte & 0x01) ? 0 : KEY_FLAG_BREAK;
                data->prev_button_state = (data->prev_button_state & ~0x01) | (byte & 0x01);
            } else {
                data->byte0 = byte;
                data->seq_state = SS_XMOVEMENT;
                advance = 1;
                status = STATUS_BUFFER_UNDERFLOW;
            }
        } else {
            advance = 1;
            status = STATUS_BUFFER_UNDERFLOW;
        }
        break;
    case SS_XMOVEMENT:
        if (data->byte0 & 0x40) break;
        if (data->byte0 & 0x10) {
            ret_key = 0x100 - byte;
            ret_flags = KEY_FLAG_XMOVE | KEY_FLAG_NEGATIVE;
        } else {
            ret_key = byte;
            ret_flags = KEY_FLAG_XMOVE;
        }
        data->seq_state = SS_YMOVEMENT;
        advance = 1;
        break;
    case SS_YMOVEMENT:
        if (data->byte0 & 0x80) break;
        if (data->byte0 & 0x20) {
            ret_key = 0x100 - byte;
            ret_flags = KEY_FLAG_YMOVE | KEY_FLAG_NEGATIVE;
        } else {
            ret_key = byte;
            ret_flags = KEY_FLAG_YMOVE;
        }
        data->seq_state = SS_DEFAULT;
        advance = 1;
        break;
    default:
        break;
    }

    data->seqbuf_start = (data->seqbuf_start + advance) % sizeof(data->seqbuf);

    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlIntP_Unmask(data->irq_num);
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (key) *key = ret_key;
    if (flags) *flags = ret_flags;

    return STATUS_SUCCESS;

has_error:
    VlIntP_Unmask(data->irq_num);

    return status;
}

static const struct hid_interface hidif = {
    .wait_event = wait_event,
    .poll_event = poll_event,
};

static void mouse_isr(void *_dev, struct VlA_InterruptFrame *frame, struct trap_regs *regs, int num)
{
    struct device *dev = _dev;
    struct ps2_mouse_data *data = (struct ps2_mouse_data *)dev->data;
    status_t status;
    uint8_t byte;

    status = data->ps2if->irq_get_byte(data->ps2dev, &byte);
    if (!CHECK_SUCCESS(status)) return;

    VlA_Out8(0x007A, 0x04);
    VlA_Out8(0x007B, byte);

    unsigned int next_seqbuf_end = (data->seqbuf_end + 1) % sizeof(data->seqbuf);
    if (next_seqbuf_end == data->seqbuf_start) return;

    data->seqbuf[data->seqbuf_end] = byte;
    data->seqbuf_end = next_seqbuf_end;
}

static status_t probe(
    struct device **devout,
    struct device_driver *drv,
    struct device *parent,
    struct resource *rsrc,
    int rsrc_cnt
);
static status_t remove(struct device *dev);
static status_t get_interface(struct device *dev, const char *name, const void **result);

static void ps2_mouse_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"ps2_mouse\"");
    }

    drv->name = "ps2_mouse";
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
    struct device *ps2dev = NULL;
    const struct ps2_interface *ps2if = NULL;
    struct ps2_mouse_data *data = NULL;
    uint8_t buf[3];

    if (!rsrc || rsrc_cnt != 2 || rsrc[0].type != RT_BUS || rsrc[0].base != rsrc[0].limit ||
        rsrc[1].type != RT_IRQ || rsrc[1].base != rsrc[1].limit) {
        return STATUS_INVALID_RESOURCE;
    }

    ps2dev = parent;
    if (!ps2dev) {
        status = STATUS_INVALID_VALUE;
        goto has_error;
    }

    status = ps2dev->driver->get_interface(ps2dev, "ps2", (const void **)&ps2if);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_Create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_GenerateName("mouse", dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->ps2dev = ps2dev;
    data->ps2if = ps2if;
    data->slave = (int)rsrc[0].base;
    data->irq_num = (int)rsrc[1].base;
    data->seqbuf_start = data->seqbuf_end = 0;
    data->seq_state = SS_DEFAULT;
    data->isr = NULL;
    dev->data = data;

    status = VlIntP_Mask(data->irq_num);
    if (!CHECK_SUCCESS(status)) goto has_error;

    LOG_DEBUG("registering interrupt service routine...\n");
    status = VlIntP_AddInterruptHandler(data->irq_num, dev, mouse_isr, &data->isr);
    if (!CHECK_SUCCESS(status)) goto has_error;

    LOG_DEBUG("testing port...\n");
    status = ps2if->test_port(ps2dev, data->slave);
    if (!CHECK_SUCCESS(status)) goto has_error;

    LOG_DEBUG("enabling port...\n");
    status = ps2if->enable_port(ps2dev, data->slave);
    if (!CHECK_SUCCESS(status)) goto has_error;

    LOG_DEBUG("resetting mouse...\n");
    /* reset device */
    buf[0] = 0xFF;

    status = ps2if->send_data(ps2dev, data->slave, buf, 1);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = ps2if->recv_data(ps2dev, data->slave, buf, 3);
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (buf[0] != 0xFA || buf[1] != 0xAA) {
        status = STATUS_HARDWARE_FAILED;
        goto has_error;
    }
    data->device_type = buf[2];

    LOG_DEBUG("enabling data reporting...\n");
    /* enable data reporting */
    buf[0] = 0xF4;

    status = ps2if->send_data(ps2dev, data->slave, buf, 1);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = ps2if->recv_data(ps2dev, data->slave, buf, 1);
    if (!CHECK_SUCCESS(status)) goto has_error;
    if (buf[0] != 0xFA) {
        status = STATUS_HARDWARE_FAILED;
        goto has_error;
    }

    status = VlIntP_Unmask(data->irq_num);
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (devout) *devout = dev;

    LOG_DEBUG("initialization success\n");

    return STATUS_SUCCESS;

has_error:
    VlIntP_Unmask((int)rsrc[1].base);

    if (data && data->isr) {
        VlIntP_RemoveHandler(data->isr);
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
    struct ps2_mouse_data *data = (struct ps2_mouse_data *)dev->data;

    VlIntP_RemoveHandler(data->isr);

    free(data);

    VlDev_Remove(dev);

    return STATUS_SUCCESS;
}

static status_t get_interface(struct device *dev, const char *name, const void **result)
{
    if (strcmp(name, "hid") == 0) {
        if (result) *result = &hidif;
        return STATUS_SUCCESS;
    }

    return STATUS_ENTRY_NOT_FOUND;
}

REGISTER_DEVICE_DRIVER(ps2_mouse, ps2_mouse_init)
