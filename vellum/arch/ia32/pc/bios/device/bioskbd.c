#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/arch/interrupt.h>

#include <vellum/plat/bios/keyboard.h>
#include <vellum/plat/panic.h>

#include <vellum/device.h>
#include <vellum/hid.h>
#include <vellum/interface/char.h>
#include <vellum/interface/hid.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/resource.h>
#include <vellum/status.h>

#define MODULE_NAME "bioskbd"

struct bioskbd_data {
    int has_pending;
    uint8_t scancode;
    char ascii;
};

static const uint16_t bios_scancode_to_key[128] = {
    [0x01] = KEY_ESC,        [0x02] = KEY_1,         [0x03] = KEY_2,
    [0x04] = KEY_3,          [0x05] = KEY_4,         [0x06] = KEY_5,
    [0x07] = KEY_6,          [0x08] = KEY_7,         [0x09] = KEY_8,
    [0x0A] = KEY_9,          [0x0B] = KEY_0,         [0x0C] = KEY_MINUS,
    [0x0D] = KEY_EQUAL,      [0x0E] = KEY_BACKSPACE, [0x0F] = KEY_TAB,
    [0x10] = KEY_Q,          [0x11] = KEY_W,         [0x12] = KEY_E,
    [0x13] = KEY_R,          [0x14] = KEY_T,         [0x15] = KEY_Y,
    [0x16] = KEY_U,          [0x17] = KEY_I,         [0x18] = KEY_O,
    [0x19] = KEY_P,          [0x1A] = KEY_LEFTBRACE, [0x1B] = KEY_RIGHTBRACE,
    [0x1C] = KEY_ENTER,      [0x1D] = KEY_LEFTCTRL,  [0x1E] = KEY_A,
    [0x1F] = KEY_S,          [0x20] = KEY_D,         [0x21] = KEY_F,
    [0x22] = KEY_G,          [0x23] = KEY_H,         [0x24] = KEY_J,
    [0x25] = KEY_K,          [0x26] = KEY_L,         [0x27] = KEY_SEMICOLON,
    [0x28] = KEY_APOSTROPHE, [0x29] = KEY_GRAVE,     [0x2A] = KEY_LEFTSHIFT,
    [0x2B] = KEY_BACKSLASH,  [0x2C] = KEY_Z,         [0x2D] = KEY_X,
    [0x2E] = KEY_C,          [0x2F] = KEY_V,         [0x30] = KEY_B,
    [0x31] = KEY_N,          [0x32] = KEY_M,         [0x33] = KEY_COMMA,
    [0x34] = KEY_DOT,        [0x35] = KEY_SLASH,     [0x36] = KEY_RIGHTSHIFT,
    [0x37] = KEY_KPASTERISK, [0x38] = KEY_LEFTALT,   [0x39] = KEY_SPACE,
    [0x3A] = KEY_CAPSLOCK,   [0x3B] = KEY_F1,        [0x3C] = KEY_F2,
    [0x3D] = KEY_F3,         [0x3E] = KEY_F4,        [0x3F] = KEY_F5,
    [0x40] = KEY_F6,         [0x41] = KEY_F7,        [0x42] = KEY_F8,
    [0x43] = KEY_F9,         [0x44] = KEY_F10,       [0x45] = KEY_NUMLOCK,
    [0x46] = KEY_SCROLLLOCK, [0x47] = KEY_KP7,       [0x48] = KEY_UP,
    [0x49] = KEY_KP9,        [0x4A] = KEY_KPMINUS,   [0x4B] = KEY_LEFT,
    [0x4C] = KEY_KP5,        [0x4D] = KEY_RIGHT,     [0x4E] = KEY_KPPLUS,
    [0x4F] = KEY_KP1,        [0x50] = KEY_DOWN,      [0x51] = KEY_KP3,
    [0x52] = KEY_INSERT,     [0x53] = KEY_DELETE,    [0x57] = KEY_F11,
    [0x58] = KEY_F12,
};

static void store_key(struct bioskbd_data *data, uint8_t scancode, char ascii)
{
    data->has_pending = 1;
    data->scancode = scancode;
    data->ascii = ascii;
}

static void wait_for_key(struct bioskbd_data *data)
{
    if (!data->has_pending) {
        VlBiosP_GetKeyboardStroke(&data->scancode, &data->ascii);
        data->has_pending = 1;
    }
}

static int poll_key(struct bioskbd_data *data)
{
    uint8_t scancode;
    char ascii;

    if (data->has_pending) return 1;
    if (VlBiosP_CheckKeyboardStroke(&scancode, &ascii)) return 0;

    store_key(data, scancode, ascii);
    return 1;
}

static VlStatus read(struct device *dev, char *buf, size_t len, size_t *result)
{
    struct bioskbd_data *data = (struct bioskbd_data *)dev->data;
    size_t read_len = 0;

    if (!buf && len) return STATUS_INVALID_VALUE;

    while (read_len < len) {
        wait_for_key(data);

        if (data->ascii) {
            buf[read_len++] = data->ascii;
        }

        data->has_pending = 0;
    }

    if (result) *result = read_len;

    return STATUS_SUCCESS;
}

static const struct char_interface charif = {
    .read = read,
};

static VlStatus wait_event(struct device *dev)
{
    struct bioskbd_data *data = (struct bioskbd_data *)dev->data;

    wait_for_key(data);

    return STATUS_SUCCESS;
}

static VlStatus poll_event(struct device *dev, uint16_t *key, uint16_t *flags)
{
    struct bioskbd_data *data = (struct bioskbd_data *)dev->data;
    uint16_t keycode;

    if (!poll_key(data)) return STATUS_NO_EVENT;

    keycode = data->scancode < ARRAY_SIZE(bios_scancode_to_key)
        ? bios_scancode_to_key[data->scancode]
        : KEY_NONE;

    data->has_pending = 0;

    if (!keycode) return STATUS_BUFFER_UNDERFLOW;

    if (key) *key = keycode;
    if (flags) *flags = 0;

    return STATUS_SUCCESS;
}

static const struct hid_interface hidif = {
    .wait_event = wait_event,
    .poll_event = poll_event,
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

static void bioskbd_init(void)
{
    VlStatus status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"bioskbd\"");
    }

    drv->name = "bioskbd";
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
    struct bioskbd_data *data = NULL;

    (void)rsrc;

    if (rsrc_cnt != 0) return STATUS_INVALID_RESOURCE;

    status = VlDev_Create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_GenerateName("kbd", dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    memset(data, 0, sizeof(*data));
    dev->data = data;

    LOG_DEBUG("initialization success\n");

    if (devout) *devout = dev;

    return STATUS_SUCCESS;

has_error:
    if (data) free(data);
    if (dev) VlDev_Remove(dev);

    return status;
}

static VlStatus remove(struct device *dev)
{
    struct bioskbd_data *data = (struct bioskbd_data *)dev->data;

    if (data) free(data);

    VlDev_Remove(dev);

    return STATUS_SUCCESS;
}

static VlStatus get_interface(struct device *dev, const char *name, const void **result)
{
    (void)dev;

    if (strcmp(name, "char") == 0) {
        if (result) *result = &charif;
        return STATUS_SUCCESS;
    }
    if (strcmp(name, "hid") == 0) {
        if (result) *result = &hidif;
        return STATUS_SUCCESS;
    }

    return STATUS_ENTRY_NOT_FOUND;
}

REGISTER_DEVICE_DRIVER(bioskbd, bioskbd_init)
