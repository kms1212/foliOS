#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <vellum/device.h>
#include <vellum/interface/char.h>
#include <vellum/interface/console.h>
#include <vellum/macros.h>

struct ansiterm_data {
    struct device *condev;
    const struct console_interface *conif;

    int escape_seq_args[8];
    int escape_seq_arg_count;
    int escape_seq_state;
    int escape_seq_number_input;
    int saved_cursor_x, saved_cursor_y;
    struct console_char_attributes attr_state;

    int utf8_fragment_len;
    char utf8_fragment_buf[6];
};

enum ansi_escape_state {
    STATE_DEFAULT = 0,

    STATE_ESC, /* -> SS2, SS3, CSI, ST, OSC, SOS, PM, APC */

    STATE_SS2,
    STATE_SS3,
    STATE_DCS,
    STATE_CSI,
    STATE_ST,
    STATE_OSC,
    STATE_SOS,
    STATE_PM,
    STATE_APC,
};

static const uint32_t palette[256] = {
    /* RGBI colors */
    0x000000,
    0xAA0000,
    0x00AA00,
    0xAA5500,
    0x0000AA,
    0xAA00AA,
    0x00AAAA,
    0xAAAAAA,
    0x555555,
    0xFF5555,
    0x55FF55,
    0xFFFF55,
    0x5555FF,
    0xFF55FF,
    0x55FFFF,
    0xFFFFFF,

    /* 216 colors */
    0x000000,
    0x00005F,
    0x000087,
    0x0000AF,
    0x0000D7,
    0x0000FF,
    0x005F00,
    0x005F5F,
    0x005F87,
    0x005FAF,
    0x005FD7,
    0x005FFF,
    0x008700,
    0x00875F,
    0x008787,
    0x0087AF,
    0x0087D7,
    0x0087FF,
    0x00AF00,
    0x00AF5F,
    0x00AF87,
    0x00AFAF,
    0x00AFD7,
    0x00AFFF,
    0x00D700,
    0x00D75F,
    0x00D787,
    0x00D7AF,
    0x00D7D7,
    0x00D7FF,
    0x00FF00,
    0x00FF5F,
    0x00FF87,
    0x00FFAF,
    0x00FFD7,
    0x00FFFF,
    0x5F0000,
    0x5F005F,
    0x5F0087,
    0x5F00AF,
    0x5F00D7,
    0x5F00FF,
    0x5F5F00,
    0x5F5F5F,
    0x5F5F87,
    0x5F5FAF,
    0x5F5FD7,
    0x5F5FFF,
    0x5F8700,
    0x5F875F,
    0x5F8787,
    0x5F87AF,
    0x5F87D7,
    0x5F87FF,
    0x5FAF00,
    0x5FAF5F,
    0x5FAF87,
    0x5FAFAF,
    0x5FAFD7,
    0x5FAFFF,
    0x5FD700,
    0x5FD75F,
    0x5FD787,
    0x5FD7AF,
    0x5FD7D7,
    0x5FD7FF,
    0x5FFF00,
    0x5FFF5F,
    0x5FFF87,
    0x5FFFAF,
    0x5FFFD7,
    0x5FFFFF,
    0x870000,
    0x87005F,
    0x870087,
    0x8700AF,
    0x8700D7,
    0x8700FF,
    0x875F00,
    0x875F5F,
    0x875F87,
    0x875FAF,
    0x875FD7,
    0x875FFF,
    0x878700,
    0x87875F,
    0x878787,
    0x8787AF,
    0x8787D7,
    0x8787FF,
    0x87AF00,
    0x87AF5F,
    0x87AF87,
    0x87AFAF,
    0x87AFD7,
    0x87AFFF,
    0x87D700,
    0x87D75F,
    0x87D787,
    0x87D7AF,
    0x87D7D7,
    0x87D7FF,
    0x87FF00,
    0x87FF5F,
    0x87FF87,
    0x87FFAF,
    0x87FFD7,
    0x87FFFF,
    0xAF0000,
    0xAF005F,
    0xAF0087,
    0xAF00AF,
    0xAF00D7,
    0xAF00FF,
    0xAF5F00,
    0xAF5F5F,
    0xAF5F87,
    0xAF5FAF,
    0xAF5FD7,
    0xAF5FFF,
    0xAF8700,
    0xAF875F,
    0xAF8787,
    0xAF87AF,
    0xAF87D7,
    0xAF87FF,
    0xAFAF00,
    0xAFAF5F,
    0xAFAF87,
    0xAFAFAF,
    0xAFAFD7,
    0xAFAFFF,
    0xAFD700,
    0xAFD75F,
    0xAFD787,
    0xAFD7AF,
    0xAFD7D7,
    0xAFD7FF,
    0xAFFF00,
    0xAFFF5F,
    0xAFFF87,
    0xAFFFAF,
    0xAFFFD7,
    0xAFFFFF,
    0xD70000,
    0xD7005F,
    0xD70087,
    0xD700AF,
    0xD700D7,
    0xD700FF,
    0xD75F00,
    0xD75F5F,
    0xD75F87,
    0xD75FAF,
    0xD75FD7,
    0xD75FFF,
    0xD78700,
    0xD7875F,
    0xD78787,
    0xD787AF,
    0xD787D7,
    0xD787FF,
    0xD7AF00,
    0xD7AF5F,
    0xD7AF87,
    0xD7AFAF,
    0xD7AFD7,
    0xD7AFFF,
    0xD7D700,
    0xD7D75F,
    0xD7D787,
    0xD7D7AF,
    0xD7D7D7,
    0xD7D7FF,
    0xD7FF00,
    0xD7FF5F,
    0xD7FF87,
    0xD7FFAF,
    0xD7FFD7,
    0xD7FFFF,
    0xFF0000,
    0xFF005F,
    0xFF0087,
    0xFF00AF,
    0xFF00D7,
    0xFF00FF,
    0xFF5F00,
    0xFF5F5F,
    0xFF5F87,
    0xFF5FAF,
    0xFF5FD7,
    0xFF5FFF,
    0xFF8700,
    0xFF875F,
    0xFF8787,
    0xFF87AF,
    0xFF87D7,
    0xFF87FF,
    0xFFAF00,
    0xFFAF5F,
    0xFFAF87,
    0xFFAFAF,
    0xFFAFD7,
    0xFFAFFF,
    0xFFD700,
    0xFFD75F,
    0xFFD787,
    0xFFD7AF,
    0xFFD7D7,
    0xFFD7FF,
    0xFFFF00,
    0xFFFF5F,
    0xFFFF87,
    0xFFFFAF,
    0xFFFFD7,
    0xFFFFFF,

    /* grayscale colors */
    0x080808,
    0x121212,
    0x1C1C1C,
    0x262626,
    0x303030,
    0x3A3A3A,
    0x444444,
    0x4E4E4E,
    0x585858,
    0x626262,
    0x6C6C6C,
    0x767676,
    0x808080,
    0x8A8A8A,
    0x949494,
    0x9E9E9E,
    0xA8A8A8,
    0xB2B2B2,
    0xBCBCBC,
    0xC6C6C6,
    0xD0D0D0,
    0xDADADA,
    0xE4E4E4,
    0xEEEEEE,
};

static status_t console_backspace(struct device *dev, int *cursor_x_out)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int width, cursor_x, cursor_y;
    struct console_char_cell *buffer = NULL;

    status = data->conif->get_cursor_pos(data->condev, &cursor_x, &cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    status = data->conif->get_dimension(data->condev, &width, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    status = data->conif->get_buffer(data->condev, &buffer);
    if (!CHECK_SUCCESS(status)) return status;

    if (cursor_x < 1) return 1;

    if (buffer[cursor_y * width + cursor_x - 1].codepoint) {
        *cursor_x_out = cursor_x - 1;
        return STATUS_SUCCESS;
    }

    for (int x = cursor_x - 2; x >= ((cursor_x - 1) & ~7); x--) {
        if (buffer[cursor_y * width + x].codepoint) {
            *cursor_x_out = x + 1;
            return STATUS_SUCCESS;
        }
    }

    *cursor_x_out = (cursor_x - 1) & ~7;
    return STATUS_SUCCESS;
}

static status_t console_erase(struct device *dev, int x0, int y0, int x1, int y1)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int width;
    struct console_char_cell *buffer = NULL;

    status = data->conif->get_dimension(data->condev, &width, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    status = data->conif->get_buffer(data->condev, &buffer);
    if (!CHECK_SUCCESS(status)) return status;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            buffer[y * width + x].attr = data->attr_state;
            buffer[y * width + x].codepoint = 0;
        }
    }

    status = data->conif->invalidate(data->condev, x0, y0, x1, y1);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static status_t console_scroll(struct device *dev, int amount, int x0, int y0, int x1, int y1)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int width;
    struct console_char_cell *buffer = NULL;

    status = data->conif->get_dimension(data->condev, &width, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    status = data->conif->get_buffer(data->condev, &buffer);
    if (!CHECK_SUCCESS(status)) return status;

    if (amount == 0) return STATUS_SUCCESS;

    if (amount > 0) {
        for (int y = y0; y <= y1 - amount; y++) {
            for (int x = x0; x <= x1; x++) {
                buffer[y * width + x] = buffer[(y + amount) * width + x];
            }
        }

        console_erase(dev, x0, y1 - amount + 1, x1, y1);
    } else {
        amount = -amount;

        for (int y = y1 - amount; y >= y0; y--) {
            for (int x = x0; x <= x1; x++) {
                buffer[(y + amount) * width + x] = buffer[y * width + x];
            }
        }

        console_erase(dev, x0, y0, x1, y0 + amount);
    }

    status = data->conif->invalidate(data->condev, x0, y0, x1, y1);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static status_t console_putchar(struct device *dev, wchar_t ch)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int x_cursor, y_cursor, width, cwidth = wcwidth(ch) > 1 ? 2 : 1;
    struct console_char_cell *buffer = NULL;

    status = data->conif->get_cursor_pos(data->condev, &x_cursor, &y_cursor);
    if (!CHECK_SUCCESS(status)) return status;

    status = data->conif->get_dimension(data->condev, &width, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    status = data->conif->get_buffer(data->condev, &buffer);
    if (!CHECK_SUCCESS(status)) return status;

    buffer[y_cursor * width + x_cursor].attr = data->attr_state;
    buffer[y_cursor * width + x_cursor].codepoint = ch;

    status = data->conif->invalidate(data->condev, x_cursor, y_cursor, x_cursor, y_cursor);
    if (!CHECK_SUCCESS(status)) return status;

    if (cwidth == 2 && x_cursor + 1 < width) {
        buffer[y_cursor * width + x_cursor + 1].attr = data->attr_state;
        buffer[y_cursor * width + x_cursor + 1].codepoint = (wchar_t)0xFFFFFFFF;

        status = data->conif->invalidate(data->condev, x_cursor, y_cursor, x_cursor, y_cursor);
        if (!CHECK_SUCCESS(status)) return status;
    }

    return STATUS_SUCCESS;
}

static status_t handle_esc(struct device *dev, char ch)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;

    for (int i = 0; i < 8; i++) {
        data->escape_seq_args[i] = 0;
    }
    data->escape_seq_arg_count = 0;
    data->escape_seq_number_input = 0;
    switch (ch) {
    case 'N':
        data->escape_seq_state = STATE_SS2;
        break;
    case 'O':
        data->escape_seq_state = STATE_SS3;
        break;
    case 'P':
        data->escape_seq_state = STATE_DCS;
        break;
    case '[':
        data->escape_seq_state = STATE_CSI;
        break;
    case '\\':
        data->escape_seq_state = STATE_ST;
        break;
    case ']':
        data->escape_seq_state = STATE_OSC;
        break;
    case 'X':
        data->escape_seq_state = STATE_SOS;
        break;
    case '^':
        data->escape_seq_state = STATE_PM;
        break;
    case '_':
        data->escape_seq_state = STATE_APC;
        break;
    default:
        data->escape_seq_state = STATE_DEFAULT;
        break;
    }

    return STATUS_SUCCESS;
}

static status_t handle_cuu(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int cursor_y;
    int count;

    status = data->conif->get_cursor_pos(data->condev, NULL, &cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    count = (data->escape_seq_arg_count < 1) ? 1 : data->escape_seq_args[0];
    count = (count < cursor_y) ? count : cursor_y;
    cursor_y -= count;
    data->escape_seq_state = STATE_DEFAULT;

    status = data->conif->set_cursor_pos(data->condev, -1, cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static status_t handle_cud(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int cursor_y, height;
    int count;

    status = data->conif->get_cursor_pos(data->condev, NULL, &cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    status = data->conif->get_dimension(data->condev, NULL, &height);
    if (!CHECK_SUCCESS(status)) return status;

    count = (data->escape_seq_arg_count < 1) ? 1 : data->escape_seq_args[0];
    count = (count < height - cursor_y) ? count : height - cursor_y;
    cursor_y += count;
    data->escape_seq_state = STATE_DEFAULT;

    status = data->conif->set_cursor_pos(data->condev, -1, cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static status_t handle_cuf(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int cursor_x, width;
    int count;

    status = data->conif->get_cursor_pos(data->condev, &cursor_x, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    status = data->conif->get_dimension(data->condev, &width, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    count = (data->escape_seq_arg_count < 1) ? 1 : data->escape_seq_args[0];
    count = (count < width - cursor_x) ? count : width - cursor_x;
    cursor_x += count;
    data->escape_seq_state = STATE_DEFAULT;

    status = data->conif->set_cursor_pos(data->condev, cursor_x, -1);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static status_t handle_cub(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int cursor_x;
    int count;

    status = data->conif->get_cursor_pos(data->condev, &cursor_x, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    count = (data->escape_seq_arg_count < 1) ? 1 : data->escape_seq_args[0];
    count = (count < cursor_x) ? count : cursor_x;
    cursor_x -= count;
    data->escape_seq_state = STATE_DEFAULT;

    status = data->conif->set_cursor_pos(data->condev, cursor_x, -1);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static status_t handle_cnl(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int cursor_x, cursor_y, height;
    int count;

    status = data->conif->get_cursor_pos(data->condev, &cursor_x, &cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    status = data->conif->get_dimension(data->condev, NULL, &height);
    if (!CHECK_SUCCESS(status)) return status;

    count = (data->escape_seq_arg_count < 1) ? 1 : data->escape_seq_args[0];
    count = (count < height - cursor_y) ? count : height - cursor_y;
    cursor_y += count;
    cursor_x = 0;
    data->escape_seq_state = STATE_DEFAULT;

    status = data->conif->set_cursor_pos(data->condev, cursor_x, cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static status_t handle_cpl(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int cursor_x, cursor_y;
    int count;

    status = data->conif->get_cursor_pos(data->condev, &cursor_x, &cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    count = (data->escape_seq_arg_count < 1) ? 1 : data->escape_seq_args[0];
    count = (count < cursor_y) ? count : cursor_y;
    cursor_y -= count;
    cursor_x = 0;
    data->escape_seq_state = STATE_DEFAULT;

    status = data->conif->set_cursor_pos(data->condev, cursor_x, cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static status_t handle_cha(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int cursor_x;
    int pos;

    status = data->conif->get_cursor_pos(data->condev, &cursor_x, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    pos = (data->escape_seq_arg_count < 1) ? 1 : data->escape_seq_args[0];
    cursor_x = pos - 1;
    if (cursor_x < 0) {
        cursor_x = 0;
    }
    data->escape_seq_state = STATE_DEFAULT;

    status = data->conif->set_cursor_pos(data->condev, cursor_x, -1);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static status_t handle_cup_hvp(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int cursor_x, cursor_y;
    int xpos, ypos;

    status = data->conif->get_cursor_pos(data->condev, &cursor_x, &cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    ypos = (data->escape_seq_arg_count < 1) ? 1 : data->escape_seq_args[0];
    xpos = (data->escape_seq_arg_count < 2) ? 1 : data->escape_seq_args[1];
    cursor_x = xpos - 1;
    if (cursor_x < 0) {
        cursor_x = 0;
    }
    cursor_y = ypos - 1;
    if (cursor_y < 0) {
        cursor_y = 0;
    }
    data->escape_seq_state = STATE_DEFAULT;

    status = data->conif->set_cursor_pos(data->condev, cursor_x, cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static status_t handle_ed(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int cursor_x, cursor_y, width, height;
    int option;

    status = data->conif->get_cursor_pos(data->condev, &cursor_x, &cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    status = data->conif->get_dimension(data->condev, &width, &height);
    if (!CHECK_SUCCESS(status)) return status;

    option = (data->escape_seq_arg_count < 1) ? 0 : data->escape_seq_args[0];
    switch (option) {
    case 0:
        status = console_erase(dev, cursor_x, cursor_y, width - 1, cursor_y);
        if (!CHECK_SUCCESS(status)) return status;

        if (cursor_y < height - 1) break;

        status = console_erase(dev, 0, cursor_y + 1, width - 1, height - 1);
        if (!CHECK_SUCCESS(status)) return status;

        break;
    case 1:
        status = console_erase(dev, 0, cursor_y, cursor_x, cursor_y);
        if (!CHECK_SUCCESS(status)) return status;

        if (cursor_x > 0) break;

        status = console_erase(dev, 0, 0, width - 1, cursor_y - 1);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 2:
    case 3:
        status = console_erase(dev, 0, 0, width - 1, height - 1);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    default:
        break;
    }
    data->escape_seq_state = STATE_DEFAULT;

    return STATUS_SUCCESS;
}

static status_t handle_el(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int cursor_x, cursor_y, width;
    int option;

    status = data->conif->get_cursor_pos(data->condev, &cursor_x, &cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    status = data->conif->get_dimension(data->condev, &width, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    option = (data->escape_seq_arg_count < 1) ? 0 : data->escape_seq_args[0];
    switch (option) {
    case 0:
        status = console_erase(dev, cursor_x, cursor_y, width - 1, cursor_y);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 1:
        status = console_erase(dev, 0, cursor_y, cursor_x, cursor_y);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 2:
        status = console_erase(dev, 0, cursor_y, width - 1, cursor_y);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    default:
        break;
    }
    data->escape_seq_state = STATE_DEFAULT;

    return STATUS_SUCCESS;
}

static status_t handle_su(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int width, height;
    int amount;

    status = data->conif->get_dimension(data->condev, &width, &height);
    if (!CHECK_SUCCESS(status)) return status;

    amount = (data->escape_seq_arg_count < 1) ? 1 : data->escape_seq_args[0];

    status = console_scroll(dev, amount, 0, 0, width - 1, height - 1);
    if (!CHECK_SUCCESS(status)) return status;

    data->escape_seq_state = STATE_DEFAULT;

    return STATUS_SUCCESS;
}

static status_t handle_sd(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int width, height;
    int amount;

    status = data->conif->get_dimension(data->condev, &width, &height);
    if (!CHECK_SUCCESS(status)) return status;

    amount = (data->escape_seq_arg_count < 1) ? 1 : data->escape_seq_args[0];

    status = console_scroll(dev, -amount, 0, 0, width - 1, height - 1);
    if (!CHECK_SUCCESS(status)) return status;

    data->escape_seq_state = STATE_DEFAULT;

    return STATUS_SUCCESS;
}

static status_t handle_sgr(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    int option;

    option = (data->escape_seq_arg_count < 1) ? 0 : data->escape_seq_args[0];
    switch (option) {
    case 0:
        data->attr_state.bg_color = palette[0];
        data->attr_state.fg_color = palette[15];
        data->attr_state.text_blink_level = 0;
        data->attr_state.text_bold = 0;
        data->attr_state.text_dim = 0;
        data->attr_state.text_italic = 0;
        data->attr_state.text_underline = 0;
        data->attr_state.text_strike = 0;
        data->attr_state.text_overlined = 0;
        data->attr_state.text_reversed = 0;
        break;
    case 1:
        data->attr_state.text_bold = 1;
        break;
    case 2:
        data->attr_state.text_dim = 1;
        break;
    case 3:
        data->attr_state.text_italic = 1;
        break;
    case 4:
        data->attr_state.text_underline = 1;
        break;
    case 5:
        data->attr_state.text_blink_level = 1;
        break;
    case 6:
        data->attr_state.text_blink_level = 2;
        break;
    case 7:
        data->attr_state.text_reversed = 1;
        break;
    case 8:
        data->attr_state.text_blink_level = 3;
        break;
    case 9:
        data->attr_state.text_strike = 1;
        break;
    case 22:
        data->attr_state.text_bold = 0;
        data->attr_state.text_dim = 0;
        break;
    case 23:
        data->attr_state.text_italic = 0;
        break;
    case 24:
        data->attr_state.text_underline = 0;
        break;
    case 25:
        data->attr_state.text_blink_level = 0;
        break;
    case 27:
        data->attr_state.text_reversed = 0;
        break;
    case 28:
        data->attr_state.text_blink_level = 0;
        break;
    case 29:
        data->attr_state.text_strike = 0;
        break;
    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
        data->attr_state.fg_color = palette[option - 30];
        break;
    case 38:
        if (data->escape_seq_arg_count < 2) break;
        switch (data->escape_seq_args[1]) {
        case 5:
            if (data->escape_seq_arg_count < 3) break;
            data->attr_state.fg_color = palette[data->escape_seq_args[2]];
            break;
        case 2:
            if (data->escape_seq_arg_count < 5) break;
            uint8_t r = data->escape_seq_args[2];
            uint8_t g = data->escape_seq_args[3];
            uint8_t b = data->escape_seq_args[4];
            data->attr_state.fg_color = (r << 16) | (g << 8) | b;
            break;
        default:
            break;
        }
        break;
    case 39:
        data->attr_state.fg_color = palette[7];
        break;
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
        data->attr_state.bg_color = palette[option - 40];
        break;
    case 48:
        if (data->escape_seq_arg_count < 2) break;
        switch (data->escape_seq_args[1]) {
        case 5:
            if (data->escape_seq_arg_count < 3) break;
            data->attr_state.bg_color = palette[data->escape_seq_args[2]];
            break;
        case 2:
            if (data->escape_seq_arg_count < 5) break;
            uint8_t r = data->escape_seq_args[2];
            uint8_t g = data->escape_seq_args[3];
            uint8_t b = data->escape_seq_args[4];
            data->attr_state.bg_color = (r << 16) | (g << 8) | b;
            break;
        default:
            break;
        }
        break;
    case 49:
        data->attr_state.bg_color = palette[0];
        break;
    case 53:
        data->attr_state.text_overlined = 1;
        break;
    case 55:
        data->attr_state.text_overlined = 0;
        break;
    case 90:
    case 91:
    case 92:
    case 93:
    case 94:
    case 95:
    case 96:
    case 97:
        data->attr_state.fg_color = palette[option - 90 + 8];
        break;
    case 100:
    case 101:
    case 102:
    case 103:
    case 104:
    case 105:
    case 106:
    case 107:
        data->attr_state.bg_color = palette[option - 100 + 8];
        break;
    default:
        break;
    }
    data->escape_seq_state = STATE_DEFAULT;

    return STATUS_SUCCESS;
}

static status_t handle_dsr(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;

    data->escape_seq_state = STATE_DEFAULT;

    return STATUS_SUCCESS;
}

static status_t handle_scosc(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int cursor_x, cursor_y;

    status = data->conif->get_cursor_pos(data->condev, &cursor_x, &cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    data->saved_cursor_x = cursor_x;
    data->saved_cursor_y = cursor_y;
    data->escape_seq_state = STATE_DEFAULT;

    return STATUS_SUCCESS;
}

static status_t handle_scorc(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;

    status = data->conif->set_cursor_pos(data->condev, data->saved_cursor_x, data->saved_cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    data->escape_seq_state = STATE_DEFAULT;

    return STATUS_SUCCESS;
}

static status_t handle_dectcem_show(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;

    status = data->conif->set_cursor_visibility(data->condev, 1);
    if (!CHECK_SUCCESS(status)) return status;

    data->escape_seq_state = STATE_DEFAULT;

    return STATUS_SUCCESS;
}

static status_t handle_dectcem_hide(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;

    status = data->conif->set_cursor_visibility(data->condev, 0);
    if (!CHECK_SUCCESS(status)) return status;

    data->escape_seq_state = STATE_DEFAULT;

    return STATUS_SUCCESS;
}

static status_t handle_csi(struct device *dev, char ch)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;

    switch (ch) {
    case 'A':
        status = handle_cuu(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 'B':
        status = handle_cud(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 'C':
        status = handle_cuf(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 'D':
        status = handle_cub(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 'E':
        status = handle_cnl(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 'F':
        status = handle_cpl(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 'G':
        status = handle_cha(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 'f':
    case 'H':
        status = handle_cup_hvp(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 'h':
        status = handle_dectcem_show(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 'J':
        status = handle_ed(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 'K':
        status = handle_el(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 'l':
        status = handle_dectcem_hide(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 'S':
        status = handle_su(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 'T':
        status = handle_sd(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 'm':
        status = handle_sgr(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 'n':
        status = handle_dsr(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 's':
        status = handle_scosc(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case 'u':
        status = handle_scorc(dev);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case '?':
        if (data->escape_seq_arg_count >= ARRAY_SIZE(data->escape_seq_args)) {
            break;
        }
        data->escape_seq_args[data->escape_seq_arg_count++] = -'?';
        break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        if (data->escape_seq_arg_count >= ARRAY_SIZE(data->escape_seq_args)) {
            break;
        }
        if (!data->escape_seq_number_input) data->escape_seq_arg_count++;
        data->escape_seq_number_input = 1;
        data->escape_seq_args[data->escape_seq_arg_count - 1] *= 10;
        data->escape_seq_args[data->escape_seq_arg_count - 1] += ch - '0';
        break;
    case ';':
    case ':':
        data->escape_seq_number_input = 0;
        break;
    default:
        data->escape_seq_state = STATE_DEFAULT;
        break;
    }

    return STATUS_SUCCESS;
}

static status_t put_char(struct device *dev, wchar_t ch)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    int cursor_x, cursor_y, width, height;

    if (data->escape_seq_state != STATE_DEFAULT) {
        switch (data->escape_seq_state) {
        case STATE_ESC:
            status = handle_esc(dev, (char)ch);
            if (!CHECK_SUCCESS(status)) return status;
            break;
        case STATE_CSI:
            status = handle_csi(dev, (char)ch);
            if (!CHECK_SUCCESS(status)) return status;
            break;
        default:
            data->escape_seq_state = STATE_DEFAULT;
            break;
        }
        return STATUS_SUCCESS;
    }

    status = data->conif->get_cursor_pos(data->condev, &cursor_x, &cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    status = data->conif->get_dimension(data->condev, &width, &height);
    if (!CHECK_SUCCESS(status)) return status;

    switch (ch) {
    case '\a':
        break;
    case '\b':
        status = console_backspace(dev, &cursor_x);
        if (!CHECK_SUCCESS(status)) return status;
        break;
    case '\t':
        status = console_erase(dev, cursor_x, cursor_y, ((cursor_x / 8) + 1) * 8, cursor_y);
        if (!CHECK_SUCCESS(status)) return status;

        cursor_x = ((cursor_x / 8) + 1) * 8;
        if (cursor_x < width) break;
        cursor_y++;
        if (cursor_y >= height) {
            cursor_y = height - 1;

            status = console_scroll(dev, 1, 0, 0, width - 1, height - 1);
            if (!CHECK_SUCCESS(status)) return status;
        }
        cursor_x = 0;
        break;
    case '\n':
        cursor_x = 0;
        cursor_y++;
        if (cursor_y < height) break;
        cursor_y = height - 1;

        status = console_scroll(dev, 1, 0, 0, width - 1, height - 1);
        if (!CHECK_SUCCESS(status)) return status;

        break;
    case '\v':
        cursor_y++;
        if (cursor_y < height) break;
        cursor_y = height - 1;

        status = console_scroll(dev, 1, 0, 0, width - 1, height - 1);
        if (!CHECK_SUCCESS(status)) return status;

        break;
    case '\f':
        break;
    case '\r':
        cursor_x = 0;
        break;
    case '\033':
        data->escape_seq_state = STATE_ESC;
        break;
    default: {
        int cwidth = wcwidth(ch);
        if (ch < 0x20) break;
        if (cwidth > 1 && cursor_x + cwidth > width) {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= height) {
                cursor_y = height - 1;

                status = console_scroll(dev, 1, 0, 0, width - 1, height - 1);
                if (!CHECK_SUCCESS(status)) return status;
            }

            status = data->conif->set_cursor_pos(data->condev, cursor_x, cursor_y);
            if (!CHECK_SUCCESS(status)) return status;
        }

        status = console_putchar(dev, ch);
        if (!CHECK_SUCCESS(status)) return status;

        cursor_x += cwidth;
        if (cursor_x < width) break;
        cursor_x = 0;
        cursor_y++;
        if (cursor_y < height) break;
        cursor_y = height - 1;

        status = console_scroll(dev, 1, 0, 0, width - 1, height - 1);
        if (!CHECK_SUCCESS(status)) return status;
    } break;
    }

    status = data->conif->set_cursor_pos(data->condev, cursor_x, cursor_y);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static int get_seq_char(struct device *dev, const char *buf, long len, int idx)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;

    if (idx < data->utf8_fragment_len) {
        return (unsigned char)data->utf8_fragment_buf[idx];
    } else if (idx < data->utf8_fragment_len + len) {
        return (unsigned char)buf[idx - data->utf8_fragment_len];
    }

    return -1;
}

static status_t write(struct device *dev, const char *buf, size_t len, size_t *result)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    size_t written_len = 0;
    int char_len, seq_char;
    wchar_t wch;

    while (len > 0) {
        seq_char = get_seq_char(dev, buf, (int)len, 0);

        if ((uint8_t)seq_char < 0x80) {
            char_len = 1;
            wch = seq_char & 0x7F;
        } else if ((uint8_t)seq_char < 0xE0) {
            char_len = 2;
            wch = (seq_char & 0x1F) << 6;
        } else if ((uint8_t)seq_char < 0xF0) {
            char_len = 3;
            wch = (seq_char & 0x0F) << 12;
        } else if ((uint8_t)seq_char < 0xF8) {
            char_len = 4;
            wch = (seq_char & 0x07) << 18;
        } else {
            char_len = -1;
            wch = 0;
        }

        if (data->utf8_fragment_len + len < char_len) {
            memcpy(data->utf8_fragment_buf + data->utf8_fragment_len, buf, len);
            data->utf8_fragment_len += (int)len;
            written_len += len;
            break;
        }

        for (int i = 1; i < char_len; i++) {
            seq_char = get_seq_char(dev, buf, (int)len, i);
            if ((seq_char & 0xC0) != 0x80) {
                wch = 0xFFFD;
                char_len = i;
                break;
            }
            wch |= (wchar_t)(seq_char & 0x3F) << ((char_len - i - 1) * 6);
        }

        if (char_len < 1) {
            wch = 0xFFFD;
            char_len = 1;
        }

        status = put_char(dev, wch);
        if (!CHECK_SUCCESS(status)) return status;

        buf += char_len - data->utf8_fragment_len;
        len -= char_len - data->utf8_fragment_len;
        written_len += char_len - data->utf8_fragment_len;
        data->utf8_fragment_len = 0;
    }

    status = data->conif->flush(data->condev);
    if (!CHECK_SUCCESS(status)) return status;

    if (result) *result = written_len;

    return STATUS_SUCCESS;
}

static const struct char_interface charif = {
    .write = write,
};

static status_t wwrite(struct device *dev, const wchar_t *buf, size_t len, size_t *result)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;
    status_t status;
    size_t written_len = 0;

    while (len > 0) {
        status = put_char(dev, *buf);
        if (!CHECK_SUCCESS(status)) return status;

        buf++;
        len--;
        written_len++;
    }

    status = data->conif->flush(data->condev);
    if (!CHECK_SUCCESS(status)) return status;

    if (result) *result = written_len;

    return STATUS_SUCCESS;
}

static const struct wchar_interface wcharif = {
    .write = wwrite,
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

static void ansiterm_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"ansiterm\"");
    }

    drv->name = "ansiterm";
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
    struct device *condev = NULL;
    const struct console_interface *conif = NULL;
    struct ansiterm_data *data = NULL;

    condev = parent;
    if (!condev) {
        status = STATUS_INVALID_VALUE;
        goto has_error;
    }

    status = condev->driver->get_interface(condev, "console", (const void **)&conif);
    if (!conif) goto has_error;

    status = VlDev_Create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_GenerateName("tty", dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->condev = condev;
    data->conif = conif;
    data->escape_seq_arg_count = 0;
    data->saved_cursor_x = 0;
    data->saved_cursor_y = 0;
    data->attr_state.fg_color = 0xffffff;
    data->attr_state.bg_color = 0x000000;
    data->attr_state.text_blink_level = 0;
    data->attr_state.text_bold = 0;
    data->attr_state.text_dim = 0;
    data->attr_state.text_italic = 0;
    data->attr_state.text_underline = 0;
    data->attr_state.text_strike = 0;
    data->attr_state.text_overlined = 0;
    data->attr_state.text_reversed = 0;
    data->utf8_fragment_len = 0;
    dev->data = data;

    conif->set_cursor_pos(condev, 0, 0);

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

static status_t remove(struct device *dev)
{
    struct ansiterm_data *data = (struct ansiterm_data *)dev->data;

    free(data);

    VlDev_Remove(dev);

    return STATUS_SUCCESS;
}

static status_t get_interface(struct device *dev, const char *name, const void **result)
{
    if (strcmp(name, "char") == 0) {
        if (result) *result = &charif;
        return STATUS_SUCCESS;
    } else if (strcmp(name, "wchar") == 0) {
        if (result) *result = &wcharif;
        return STATUS_SUCCESS;
    }

    return STATUS_ENTRY_NOT_FOUND;
}

REGISTER_DEVICE_DRIVER(ansiterm, ansiterm_init)
