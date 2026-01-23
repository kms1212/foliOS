#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <vellum/asm/isr.h>
#include <vellum/asm/io.h>
#include <vellum/asm/time.h>
#include <vellum/asm/intrinsics/misc.h>

#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/status.h>
#include <vellum/device.h>
#include <vellum/hid.h>
#include <vellum/debug.h>
#include <vellum/interface/char.h>
#include <vellum/interface/hid.h>
#include <vellum/interface/ps2.h>

#define MODULE_NAME "ps2kbd"

enum sequence_state {
    SS_DEFAULT = 0,
    SS_EXTENDED,
    SS_BREAK,
    SS_BREAK_EXTENDED,
    SS_PAUSE_1,
    SS_PAUSE_2,
    SS_PAUSE_3,
    SS_PAUSE_4,
    SS_PAUSE_5,
    SS_PAUSE_6,
    SS_PAUSE_7,
    SS_CTRLPAUSE_1,
    SS_CTRLPAUSE_2,
    SS_CTRLPAUSE_3,
};

struct ps2_keyboard_data {
    struct device *ps2dev;
    const struct ps2_interface *ps2if;

    int irq_num;
    struct isr_handler *isr;

    volatile int seqbuf_start, seqbuf_end;
    volatile uint8_t seqbuf[64];
    enum sequence_state seq_state;
    int scancode_set;
};

struct key_event {
    uint16_t keycode;
    uint16_t flags;
};

static const uint16_t translated_keycode_char_table[256] = {
    '\0', '\0', '\0', '\0', 'a' , 'b' , 'c' , 'd' ,  /* 0x00 - 0x07 */
    'e' , 'f' , 'g' , 'h' , 'i' , 'j' , 'k' , 'l' ,  /* 0x08 - 0x0F */
    'm' , 'n' , 'o' , 'p' , 'q' , 'r' , 's' , 't' ,  /* 0x10 - 0x17 */
    'u' , 'v' , 'w' , 'x' , 'y' , 'z' , '1' , '2' ,  /* 0x18 - 0x1F */
    '3' , '4' , '5' , '6' , '7' , '8' , '9' , '0' ,  /* 0x20 - 0x27 */
    '\n', '\0', '\b', '\t', ' ' , '-' , '=' , '[' ,  /* 0x28 - 0x2F */
    ']' , '\\', '#' , ':' , '"' , '$' , ',' , '.' ,  /* 0x30 - 0x37 */
    '/' , '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0x38 - 0x3F */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0x40 - 0x47 */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0x48 - 0x4F */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0x50 - 0x57 */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0x58 - 0x5F */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0x60 - 0x67 */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0x68 - 0x6F */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0x70 - 0x77 */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0x78 - 0x7F */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0x80 - 0x87 */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0x88 - 0x8F */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0x90 - 0x97 */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0x98 - 0x9F */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0xA0 - 0xA7 */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0xA8 - 0xAF */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0xB0 - 0xB7 */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0xB8 - 0xBF */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0xC0 - 0xC7 */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0xC8 - 0xCF */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0xD0 - 0xD7 */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0xD8 - 0xDF */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0xE0 - 0xE7 */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0xE8 - 0xEF */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0xF0 - 0xF7 */
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  /* 0xF8 - 0xFF */
};

static const uint16_t code_table_set1_noext_low[128] = {
    KEY_NONE,       KEY_ESC,        KEY_1,          KEY_2,
    KEY_3,          KEY_4,          KEY_5,          KEY_6,
    KEY_7,          KEY_8,          KEY_9,          KEY_0,
    KEY_MINUS,      KEY_EQUAL,      KEY_BACKSPACE,  KEY_TAB,
    KEY_Q,          KEY_W,          KEY_E,          KEY_R,
    KEY_T,          KEY_Y,          KEY_U,          KEY_I,
    KEY_O,          KEY_P,          KEY_LEFTBRACE,  KEY_RIGHTBRACE,
    KEY_ENTER,      KEY_LEFTCTRL,   KEY_A,          KEY_S,
    KEY_D,          KEY_F,          KEY_G,          KEY_H,
    KEY_J,          KEY_K,          KEY_L,          KEY_SEMICOLON,
    KEY_APOSTROPHE, KEY_GRAVE,      KEY_LEFTSHIFT,  KEY_BACKSLASH,
    KEY_Z,          KEY_X,          KEY_C,          KEY_V,
    KEY_B,          KEY_N,          KEY_M,          KEY_COMMA,
    KEY_DOT,        KEY_SLASH,      KEY_RIGHTSHIFT, KEY_KPASTERISK,
    KEY_LEFTALT,    KEY_SPACE,      KEY_CAPSLOCK,   KEY_F1,
    KEY_F2,         KEY_F3,         KEY_F4,         KEY_F5,
    KEY_F6,         KEY_F7,         KEY_F8,         KEY_F9,
    KEY_F10,        KEY_NUMLOCK,    KEY_SCROLLLOCK, KEY_KP7,
    KEY_KP8,        KEY_KP9,        KEY_KPMINUS,    KEY_KP4,
    KEY_KP5,        KEY_KP6,        KEY_KPPLUS,     KEY_KP1,
    KEY_KP2,        KEY_KP3,        KEY_KP0,        KEY_KPDOT,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_F11,
    KEY_F12,        KEY_KPEQUAL,    KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_F13,        KEY_F14,        KEY_F15,        KEY_F16,
    KEY_F17,        KEY_F18,        KEY_F19,        KEY_F20,
    KEY_F21,        KEY_F22,        KEY_F23,        KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_F24,        KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
};

static const uint16_t code_table_set1_ext_low[128] = { KEY_NONE, };

static const uint16_t code_table_set2_noext_low[132] = {
    KEY_ERR_OVF,    KEY_F9,         KEY_NONE,       KEY_F5,
    KEY_F3,         KEY_F1,         KEY_F2,         KEY_F12,
    KEY_F13,        KEY_F10,        KEY_F8,         KEY_F6,
    KEY_F4,         KEY_TAB,        KEY_GRAVE,      KEY_KPEQUAL,
    KEY_F14,        KEY_LEFTALT,    KEY_LEFTSHIFT,  KEY_NONE,
    KEY_LEFTCTRL,   KEY_Q,          KEY_1,          KEY_NONE,
    KEY_F15,        KEY_NONE,       KEY_Z,          KEY_S,
    KEY_A,          KEY_W,          KEY_2,          KEY_NONE,
    KEY_F16,        KEY_C,          KEY_X,          KEY_D,
    KEY_E,          KEY_4,          KEY_3,          KEY_NONE,
    KEY_F17,        KEY_SPACE,      KEY_V,          KEY_F,
    KEY_T,          KEY_R,          KEY_5,          KEY_NONE,
    KEY_F18,        KEY_N,          KEY_B,          KEY_H,
    KEY_G,          KEY_Y,          KEY_6,          KEY_NONE,
    KEY_F19,        KEY_NONE,       KEY_M,          KEY_J,
    KEY_U,          KEY_7,          KEY_8,          KEY_NONE,
    KEY_F20,        KEY_COMMA,      KEY_K,          KEY_I,
    KEY_O,          KEY_0,          KEY_9,          KEY_NONE,
    KEY_F21,        KEY_DOT,        KEY_SLASH,      KEY_L,
    KEY_SEMICOLON,  KEY_P,          KEY_MINUS,      KEY_NONE,
    KEY_F22,        KEY_NONE,       KEY_APOSTROPHE, KEY_NONE,
    KEY_LEFTBRACE,  KEY_EQUAL,      KEY_NONE,       KEY_F23,
    KEY_CAPSLOCK,   KEY_RIGHTSHIFT, KEY_ENTER,      KEY_RIGHTBRACE,
    KEY_NONE,       KEY_BACKSLASH,  KEY_NONE,       KEY_F24,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_BACKSPACE,  KEY_NONE,
    KEY_NONE,       KEY_KP1,        KEY_NONE,       KEY_KP4,
    KEY_KP7,        KEY_KPCOMMA,    KEY_NONE,       KEY_NONE,
    KEY_KP0,        KEY_KPDOT,      KEY_KP2,        KEY_KP5,
    KEY_KP6,        KEY_KP8,        KEY_ESC,        KEY_NUMLOCK,
    KEY_F11,        KEY_KPPLUS,     KEY_KP3,        KEY_KPMINUS,
    KEY_KPASTERISK, KEY_KP9,        KEY_SCROLLLOCK, KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_F7,
};

static const uint16_t code_table_set2_ext_low[128] = {
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_RIGHTALT,   KEY_NONE,       KEY_NONE,
    KEY_RIGHTCTRL,  KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_LEFTMETA,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_RIGHTMETA,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_POWER,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_MEDIA_SLEEP,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_KPENTER,    KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_MEDIA_COFFEE, KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_END,        KEY_NONE,       KEY_LEFT,
    KEY_HOME,       KEY_NONE,       KEY_NONE,       KEY_NONE,
    KEY_INSERT,     KEY_DELETE,     KEY_DOWN,       KEY_NONE,
    KEY_RIGHT,      KEY_UP,         KEY_NONE,       KEY_NONE,
    KEY_NONE,       KEY_NONE,       KEY_PAGEDOWN,   KEY_NONE,
    KEY_NONE,       KEY_PAGEUP,     KEY_NONE,       KEY_NONE,
};

static int translate_scancode_set1(struct device *dev, uint16_t *keycode, uint16_t *flags)
{
    struct ps2_keyboard_data *data = (struct ps2_keyboard_data *)dev->data;
    int ret = 0;
    uint16_t ret_keycode = KEY_NONE, ret_flags = 0;

    uint8_t byte = data->seqbuf[data->seqbuf_start];
    switch (byte) {
        case 0xE0:  /* extend */
            data->seq_state = SS_EXTENDED;
            break;
        case 0xE1:  /* pause */
            data->seq_state = SS_PAUSE_1;
            break;
        default:
            switch (data->seq_state) {
                case SS_DEFAULT:
                    if (byte == 0xF1) {
                        /* LANG2 */
                    } else if (byte == 0xF2) {
                        /* LANG1 */
                    } else if (byte == 0xFC) {
                        /* POST Fail */
                    } else if (byte < sizeof(code_table_set1_noext_low)) {
                        ret_keycode = code_table_set1_noext_low[byte];
                        ret = 0;
                    } else if (byte < 128 + sizeof(code_table_set1_noext_low)) {
                        ret_flags |= KEY_FLAG_BREAK;
                        ret_keycode = code_table_set1_noext_low[byte - 128];
                        ret = 0;
                    }
                    data->seq_state = SS_DEFAULT;
                    break;
                case SS_EXTENDED:
                    if (byte == 0x46) {
                        data->seq_state = SS_CTRLPAUSE_1;
                    } else if (byte < sizeof(code_table_set1_ext_low)) {
                        ret_keycode = code_table_set1_ext_low[byte];
                        ret = 0;
                        data->seq_state = SS_DEFAULT;
                    } else if (byte < 128 + sizeof(code_table_set1_ext_low)) {
                        ret_flags |= KEY_FLAG_BREAK;
                        ret_keycode = code_table_set1_ext_low[byte - 128];
                        ret = 0;
                        data->seq_state = SS_DEFAULT;
                    }
                    break;
                case SS_PAUSE_1:
                    if (byte == 0x1D) {
                        data->seq_state = SS_PAUSE_2;
                    } else {
                        data->seq_state = SS_DEFAULT;
                    }
                    break;
                case SS_PAUSE_2:
                    if (byte == 0x45) {
                        data->seq_state = SS_PAUSE_3;
                    } else {
                        data->seq_state = SS_DEFAULT;
                    }
                    break;
                case SS_PAUSE_3:
                    if (byte == 0xE1) {
                        data->seq_state = SS_PAUSE_4;
                    } else {
                        data->seq_state = SS_DEFAULT;
                    }
                    break;
                case SS_PAUSE_4:
                    if (byte == 0x9D) {
                        data->seq_state = SS_PAUSE_5;
                    } else {
                        data->seq_state = SS_DEFAULT;
                    }
                    break;
                case SS_PAUSE_5:
                    if (byte == 0xC5) {
                        ret_keycode = KEY_PAUSE;
                        ret = 0;
                    }
                    data->seq_state = SS_DEFAULT;
                    break;
                case SS_CTRLPAUSE_1:
                    if (byte == 0xE0) {
                        data->seq_state = SS_CTRLPAUSE_2;
                    } else {
                        data->seq_state = SS_DEFAULT;
                    }
                    break;
                case SS_CTRLPAUSE_2:
                    if (byte == 0xC6) {
                        ret_keycode = KEY_PAUSE;
                        ret_flags |= KEY_FLAG_BREAK;
                        ret = 0;
                    }
                    data->seq_state = SS_DEFAULT;
                    break;
                default:
                    data->seq_state = SS_DEFAULT;
                    break;
            }
            break;
    }

    data->seqbuf_start = (data->seqbuf_start + 1) % sizeof(data->seqbuf);

    if (!ret) {
        *keycode = ret_keycode;
        *flags = ret_flags;
    }

    return ret;
}

static int translate_scancode_set2(struct device *dev, uint16_t *keycode, uint16_t *flags)
{
    struct ps2_keyboard_data *data = (struct ps2_keyboard_data *)dev->data;

    int ret = 1;
    uint16_t ret_keycode = KEY_NONE, ret_flags = 0;

    uint8_t byte = data->seqbuf[data->seqbuf_start];
    switch (byte) {
        case 0xF0:  /* break */
            if (data->seq_state == SS_EXTENDED) {
                data->seq_state = SS_BREAK_EXTENDED;
            } else {
                data->seq_state = SS_BREAK;
            }
            break;
        case 0xE0:  /* extend */
            data->seq_state = SS_EXTENDED;
            break;
        case 0xE1:  /* pause */
            data->seq_state = SS_PAUSE_1;
            break;
        default:
            switch (data->seq_state) {
                case SS_BREAK:
                    ret_flags |= KEY_FLAG_BREAK;
                case SS_DEFAULT:
                    if (byte == 0xF1) {
                        /* LANG2 */
                    } else if (byte == 0xF2) {
                        /* LANG1 */
                    } else if (byte == 0xFC) {
                        /* POST Fail */
                    } else if (byte < sizeof(code_table_set2_noext_low)) {
                        ret_keycode = code_table_set2_noext_low[byte];
                        ret = 0;
                    }
                    data->seq_state = SS_DEFAULT;
                    break;
                case SS_BREAK_EXTENDED:
                    ret_flags |= KEY_FLAG_BREAK;
                case SS_EXTENDED:
                    if (byte == 0x7E) {
                        data->seq_state = SS_CTRLPAUSE_1;
                    } if (byte < sizeof(code_table_set2_ext_low)) {
                        ret_keycode = code_table_set2_ext_low[byte];
                        ret = 0;
                        data->seq_state = SS_DEFAULT;
                    }
                    break;
                case SS_PAUSE_1:
                    if (byte == 0x14) {
                        data->seq_state = SS_PAUSE_2;
                    } else {
                        data->seq_state = SS_DEFAULT;
                    }
                    break;
                case SS_PAUSE_2:
                    if (byte == 0x77) {
                        data->seq_state = SS_PAUSE_3;
                    } else {
                        data->seq_state = SS_DEFAULT;
                    }
                    break;
                case SS_PAUSE_3:
                    if (byte == 0xE1) {
                        data->seq_state = SS_PAUSE_4;
                    } else {
                        data->seq_state = SS_DEFAULT;
                    }
                    break;
                case SS_PAUSE_4:
                    if (byte == 0xF0) {
                        data->seq_state = SS_PAUSE_5;
                    } else {
                        data->seq_state = SS_DEFAULT;
                    }
                    break;
                case SS_PAUSE_5:
                    if (byte == 0x14) {
                        data->seq_state = SS_PAUSE_6;
                    } else {
                        data->seq_state = SS_DEFAULT;
                    }
                    break;
                case SS_PAUSE_6:
                    if (byte == 0xF0) {
                        data->seq_state = SS_PAUSE_7;
                    } else {
                        data->seq_state = SS_DEFAULT;
                    }
                    break;
                case SS_PAUSE_7:
                    if (byte == 0x77) {
                        ret_keycode = KEY_PAUSE;
                        ret = 0;
                    }
                    data->seq_state = SS_DEFAULT;
                    break;
                case SS_CTRLPAUSE_1:
                    if (byte == 0xE0) {
                        data->seq_state = SS_CTRLPAUSE_2;
                    } else {
                        data->seq_state = SS_DEFAULT;
                    }
                    break;
                case SS_CTRLPAUSE_2:
                    if (byte == 0xF0) {
                        data->seq_state = SS_CTRLPAUSE_3;
                    } else {
                        data->seq_state = SS_DEFAULT;
                    }
                    break;
                case SS_CTRLPAUSE_3:
                    if (byte == 0x7E) {
                        ret_keycode = KEY_PAUSE;
                        ret_flags |= KEY_FLAG_BREAK;
                        ret = 0;
                    }
                    data->seq_state = SS_DEFAULT;
                    break;
                default:
                    data->seq_state = SS_DEFAULT;
                    break;
            }
            break;
    }

    data->seqbuf_start = (data->seqbuf_start + 1) % sizeof(data->seqbuf);

    if (!ret) {
        *keycode = ret_keycode;
        *flags = ret_flags;
    }

    return ret;
}

static int translate_scancode(struct device *dev)
{
    struct ps2_keyboard_data *data = (struct ps2_keyboard_data *)dev->data;

    uint16_t translated, flags;
    char ch = 0;
    int ret;

    do {
        while (data->seqbuf_start == data->seqbuf_end) {
            _ia32_pause();
        }

        _pc_isr_mask_interrupt(data->irq_num);

        if (data->scancode_set == 1) {
            ret = translate_scancode_set1(dev, &translated, &flags);
        } else if (data->scancode_set == 2) {
            ret = translate_scancode_set2(dev, &translated, &flags);
        }

        _pc_isr_unmask_interrupt(data->irq_num);
    } while (ret);

    if (translated < sizeof(translated_keycode_char_table)) {
        ch = translated_keycode_char_table[translated];
    }

    return (flags & KEY_FLAG_BREAK) ? 0 : ch;
}

static status_t read(struct device *dev, char *buf, size_t len, size_t *result)
{
    size_t read_len = 0;
    int ch;

    while (read_len < len) {
        ch = translate_scancode(dev);
        if (ch) {
            *buf++ = ch;
            read_len++;
        }
    }

    if (result) *result = read_len;

    return STATUS_SUCCESS;
}

static const struct char_interface charif = {
    .read = read,
};

static status_t wait_event(struct device *dev)
{
    struct ps2_keyboard_data *data = (struct ps2_keyboard_data *)dev->data;

    while (data->seqbuf_start == data->seqbuf_end) {
        _ia32_pause();
    }

    return STATUS_SUCCESS;
}

static status_t poll_event(struct device *dev, uint16_t *key, uint16_t *flags)
{
    struct ps2_keyboard_data *data = (struct ps2_keyboard_data *)dev->data;
    status_t status;

    if (data->seqbuf_start == data->seqbuf_end) return STATUS_NO_EVENT;

    status = _pc_isr_mask_interrupt(data->irq_num);
    if (!CHECK_SUCCESS(status)) goto has_error;

    switch (data->scancode_set) {
        case 1:
            status = translate_scancode_set1(dev, key, flags) ? STATUS_BUFFER_UNDERFLOW : STATUS_SUCCESS;
        case 2:
            status = translate_scancode_set2(dev, key, flags) ? STATUS_BUFFER_UNDERFLOW : STATUS_SUCCESS;
        default:
            break;
    }
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = _pc_isr_unmask_interrupt(data->irq_num);
    if (!CHECK_SUCCESS(status)) goto has_error;

    return STATUS_SUCCESS;

has_error:
    _pc_isr_unmask_interrupt(data->irq_num);

    return status;
}

static const struct hid_interface hidif = {
    .wait_event = wait_event,
    .poll_event = poll_event,
};

static void keyboard_isr(void *_dev, struct interrupt_frame *frame, struct trap_regs *regs, int num)
{
    struct device *dev = _dev;
    struct ps2_keyboard_data *data = (struct ps2_keyboard_data *)dev->data;
    status_t status;
    uint8_t byte;
    int next_seqbuf_end;

    status = data->ps2if->irq_get_byte(data->ps2dev, &byte);
    if (!CHECK_SUCCESS(status)) return;

    io_out8(0x007A, 0x02);
    io_out8(0x007B, byte);
    
    next_seqbuf_end = (data->seqbuf_end + 1) % sizeof(data->seqbuf);
    if (next_seqbuf_end == data->seqbuf_start) return;

    data->seqbuf[data->seqbuf_end] = byte;
    data->seqbuf_end = next_seqbuf_end;
}

static status_t probe(struct device **devout, struct device_driver *drv, struct device *parent, struct resource *rsrc, int rsrc_cnt);
static status_t remove(struct device *dev);
static status_t get_interface(struct device *dev, const char *name, const void **result);

static void ps2_keyboard_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = device_driver_create(&drv);
    if (!CHECK_SUCCESS(status)) {
        panic(status, "cannot register device driver \"ps2_keyboard\"");
    }

    drv->name = "ps2_keyboard";
    drv->probe = probe;
    drv->remove = remove;
    drv->get_interface = get_interface;
}

static status_t probe(struct device **devout, struct device_driver *drv, struct device *parent, struct resource *rsrc, int rsrc_cnt)
{
    status_t status;
    struct device *dev = NULL;
    struct device *ps2dev = NULL;
    const struct ps2_interface *ps2if = NULL;
    struct ps2_keyboard_data *data = NULL;
    uint8_t buf[2];

    if (!rsrc || rsrc_cnt != 2 ||
        rsrc[0].type != RT_BUS || rsrc[0].base != rsrc[0].limit ||
        rsrc[1].type != RT_IRQ || rsrc[1].base != rsrc[1].limit) {
        status = STATUS_INVALID_RESOURCE;
        goto has_error;
    }

    ps2dev = parent;
    if (!ps2dev) {
        status = STATUS_INVALID_VALUE;
        goto has_error;
    }

    status = ps2dev->driver->get_interface(ps2dev, "ps2", (const void **)&ps2if);
    if (!CHECK_SUCCESS(status)) goto has_error;
    
    status = device_create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = device_generate_name("kbd", dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->ps2dev = ps2dev;
    data->ps2if = ps2if;
    data->seqbuf_start = data->seqbuf_end = 0;
    data->seq_state = SS_DEFAULT;
    data->scancode_set = 1;
    data->irq_num = rsrc[1].base;
    data->isr = NULL;
    dev->data = data;

    status = _pc_isr_mask_interrupt(rsrc[1].base);
    if (!CHECK_SUCCESS(status)) goto has_error;
    
    LOG_DEBUG("registering interrupt service routine...\n");
    status = _pc_isr_add_interrupt_handler(rsrc[1].base, dev, keyboard_isr, &data->isr);
    if (!CHECK_SUCCESS(status)) goto has_error;
    
    LOG_DEBUG("testing port...\n");
    status = ps2if->test_port(ps2dev, rsrc[0].base);
    if (!CHECK_SUCCESS(status)) goto has_error;

    LOG_DEBUG("enabling port...\n");
    status = ps2if->enable_port(ps2dev, rsrc[0].base);
    if (!CHECK_SUCCESS(status)) goto has_error;

    LOG_DEBUG("resetting keyboard...\n");
    /* reset device */
    buf[0] = 0xFF;

    status = ps2if->send_data(ps2dev, rsrc[0].base, buf, 1);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = ps2if->recv_data(ps2dev, rsrc[0].base, buf, 2);
    if (!CHECK_SUCCESS(status)) goto has_error;
    if (buf[0] != 0xFA || buf[1] != 0xAA) {
        status = STATUS_HARDWARE_FAILED;
        goto has_error;
    }

    LOG_DEBUG("trying scancode set 2...\n");
    /* try scan code set 2 */
    buf[0] = 0xF0;
    buf[1] = 0x02;

    status = ps2if->send_data(ps2dev, rsrc[0].base, buf, 2);
    if (!CHECK_SUCCESS(status)) goto skip_set2;

    status = ps2if->recv_data(ps2dev, rsrc[0].base, buf, 1);
    if (!CHECK_SUCCESS(status)) goto skip_set2;
    if (buf[0] != 0xFA) goto skip_set2;

    data->scancode_set = 2;

skip_set2:
    status = _pc_isr_unmask_interrupt(rsrc[1].base);
    if (!CHECK_SUCCESS(status)) goto has_error;

    LOG_DEBUG("initialization success\n");
    
    if (devout) *devout = dev;

    return STATUS_SUCCESS;

has_error:
    _pc_isr_unmask_interrupt(rsrc[1].base);

    if (data && data->isr) {
        _pc_isr_remove_handler(data->isr);
    }

    if (data) {
        free(data);
    }

    if (dev) {
        device_remove(dev);
    }

    fprintf(stderr, "%08X\n", status);

    return status;
}

static status_t remove(struct device *dev)
{
    struct ps2_keyboard_data *data = (struct ps2_keyboard_data *)dev->data;

    _pc_isr_remove_handler(data->isr);

    free(data);

    device_remove(dev);

    return STATUS_SUCCESS;
}

static status_t get_interface(struct device *dev, const char *name, const void **result)
{
    if (strcmp(name, "char") == 0) {
        if (result) *result = &charif;
        return STATUS_SUCCESS;
    } else if (strcmp(name, "hid") == 0) {
        if (result) *result = &hidif;
        return STATUS_SUCCESS;
    }

    return STATUS_ENTRY_NOT_FOUND;
}

REGISTER_DEVICE_DRIVER(ps2_keyboard, ps2_keyboard_init)
