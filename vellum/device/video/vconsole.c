#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <vellum/arch/io.h>

#include <vellum/compiler.h>
#include <vellum/device.h>
#include <vellum/font.h>
#include <vellum/interface/char.h>
#include <vellum/interface/console.h>
#include <vellum/interface/framebuffer.h>
#include <vellum/interface/video.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/status.h>

#define MODULE_NAME "vconsole"

struct vconsole_data {
    struct device *fbdev;
    const struct video_interface *vidif;
    const struct framebuffer_interface *fbif;
    const struct console_interface *conif;

    int is_switching_mode;
    int current_vmode;
    struct video_mode_info current_vmode_info;
    int cols, rows;
    int fbdev_callback_id;

    int cursor_col, cursor_row;
    int cursor_prev_col, cursor_prev_row;
    int cursor_visible;

    struct console_char_cell *char_buffer;
    uint8_t *diff_buffer;
};

inline static int get_glyph_bit(const uint8_t *glyph_data, int cwidth, int x, int y)
{
    return glyph_data[(y * cwidth + x) / 8] & (0x80 >> ((y * cwidth + x) % 8));
}

static status_t draw_char(struct device *dev, int col, int row)
{
    struct vconsole_data *data = (struct vconsole_data *)dev->data;
    status_t status;
    uint32_t *framebuffer;
    struct console_char_cell *cell;
    int cwidth, cheight;
    int x_offset, y_offset;
    uint8_t font_glyph[32];
    static const uint8_t fallback_glyph[16] = {
        0x00,
        0x00,
        0x00,
        0x7E,
        0x66,
        0x5A,
        0x5A,
        0x7A,
        0x76,
        0x76,
        0x7E,
        0x76,
        0x76,
        0x7E,
        0x00,
        0x00,
    };
    const uint8_t *glyph;
    int use_fallback;

    status = data->fbif->get_framebuffer(data->fbdev, (void **)&framebuffer);
    if (!CHECK_SUCCESS(status)) return status;

    cell = &data->char_buffer[row * data->cols + col];
    if (cell->codepoint == 0xFFFFFFFF) {
        return STATUS_SUCCESS;
    }

    x_offset = col * 8;
    y_offset = row * 16;

    use_fallback = 0;
    status = VlFont_GetGlyphDimension(cell->codepoint, &cwidth, &cheight);
    if (!CHECK_SUCCESS(status) || (cwidth != 8 && cwidth != 16) || cheight != 16) {
        use_fallback = 1;
    }

    if (!use_fallback) {
        status = VlFont_GetGlyphData(cell->codepoint, font_glyph, sizeof(font_glyph));
        if (!CHECK_SUCCESS(status)) {
            use_fallback = 1;
        }
    }

    if (use_fallback) {
        cwidth = 8;
        cheight = 16;
    }

    glyph = use_fallback ? fallback_glyph : font_glyph;

    for (int y = 0; y < cheight; y++) {
        for (int x = 0; x < cwidth; x++) {
            int fg = get_glyph_bit(glyph, cwidth, x, y);
            if (cell->attr.text_bold && x > 0) {
                fg |= get_glyph_bit(glyph, cwidth, x - 1, y);
            }

            uint32_t fg_color = cell->attr.fg_color;
            uint32_t bg_color = cell->attr.bg_color;

            if (cell->attr.text_dim) {
                fg_color &= 0xFCFCFC;
                fg_color = (fg_color >> 1) + (fg_color >> 2);
                bg_color &= 0xFCFCFC;
                bg_color = (bg_color >> 1) + (bg_color >> 2);
            }

            if (cell->attr.text_reversed) {
                framebuffer[(y_offset + y) * data->current_vmode_info.width + x_offset + x] =
                    fg ? bg_color : fg_color;
            } else {
                framebuffer[(y_offset + y) * data->current_vmode_info.width + x_offset + x] =
                    fg ? fg_color : bg_color;
            }
        }

        if ((cell->attr.text_overlined && y == 1) || (cell->attr.text_strike && y == 8) ||
            (cell->attr.text_underline && y == 15)) {
            for (int x = 0; x < cwidth; x++) {
                framebuffer[(y_offset + y) * data->current_vmode_info.width + x_offset + x] =
                    cell->attr.text_reversed ? cell->attr.bg_color : cell->attr.fg_color;
            }
        }
    }

    status =
        data->fbif
            ->invalidate(data->fbdev, x_offset, y_offset, x_offset + cwidth, y_offset + cheight);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static status_t draw_cursor(struct device *dev)
{
    struct vconsole_data *data = (struct vconsole_data *)dev->data;
    status_t status;
    uint32_t *framebuffer;
    int x_offset, y_offset;

    status = data->fbif->get_framebuffer(data->fbdev, (void **)&framebuffer);
    if (!CHECK_SUCCESS(status)) return status;

    x_offset = data->cursor_col * 8;
    y_offset = data->cursor_row * 16;

    for (int y = y_offset + 14; y < y_offset + 16; y++) {
        for (int x = x_offset; x < x_offset + 8; x++) {
            framebuffer[y * data->current_vmode_info.width + x] = 0xFFFFFF;
        }
    }

    status =
        data->fbif->invalidate(data->fbdev, x_offset, y_offset + 14, x_offset + 8, y_offset + 16);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static status_t get_dimension(struct device *dev, int *width, int *height)
{
    struct vconsole_data *data = (struct vconsole_data *)dev->data;

    if (data->current_vmode_info.text) {
        return data->conif->get_dimension(data->fbdev, width, height);
    }

    if (width) *width = data->cols;
    if (height) *height = data->rows;

    return STATUS_SUCCESS;
}

static status_t get_buffer(struct device *dev, struct console_char_cell **buf)
{
    struct vconsole_data *data = (struct vconsole_data *)dev->data;

    if (data->current_vmode_info.text) {
        return data->conif->get_buffer(data->fbdev, buf);
    }

    if (buf) *buf = data->char_buffer;

    return STATUS_SUCCESS;
}

static status_t invalidate(struct device *dev, int x0, int y0, int x1, int y1)
{
    struct vconsole_data *data = (struct vconsole_data *)dev->data;

    if (data->current_vmode_info.text) {
        return data->conif->invalidate(data->fbdev, x0, y0, x1, y1);
    }

    if (data->is_switching_mode) return STATUS_CONFLICTING_STATE;

    for (int row = y0; row <= y1; row++) {
        for (int col = x0; col <= x1; col++) {
            data->diff_buffer[(row * data->cols + col) / 8] |= 1 << ((row * data->cols + col) % 8);
        }
    }

    return STATUS_SUCCESS;
}

static status_t flush(struct device *dev)
{
    struct vconsole_data *data = (struct vconsole_data *)dev->data;
    status_t status;

    if (data->current_vmode_info.text) {
        return data->conif->flush(data->fbdev);
    }

    if (data->is_switching_mode) return STATUS_CONFLICTING_STATE;

    for (int row = 0; row < data->rows; row++) {
        for (int col = 0; col < data->cols; col++) {
            if (data->diff_buffer[(row * data->cols + col) / 8] &
                    (1 << ((row * data->cols + col) % 8)) ||
                ((data->cursor_prev_col != data->cursor_col ||
                  data->cursor_prev_row != data->cursor_row) &&
                 (data->cursor_prev_col == col && data->cursor_prev_row == row))) {
                /* status = */ draw_char(dev, col, row);
                // ignore status

                data->diff_buffer[(row * data->cols + col) / 8] &=
                    ~(1 << ((row * data->cols + col) % 8));
            }
        }
    }

    if (data->cursor_prev_col != data->cursor_col || data->cursor_prev_row != data->cursor_row) {
        data->cursor_prev_col = data->cursor_col;
        data->cursor_prev_row = data->cursor_row;

        if (data->cursor_visible) {
            /* status = */ draw_cursor(dev);
            // ignore status
        }
    }

    status = data->fbif->flush(data->fbdev);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

static status_t set_cursor_pos(struct device *dev, int col, int row)
{
    struct vconsole_data *data = (struct vconsole_data *)dev->data;

    if (data->current_vmode_info.text) {
        return data->conif->set_cursor_pos(data->fbdev, col, row);
    }

    data->cursor_col = col;
    data->cursor_row = row;

    return STATUS_SUCCESS;
}

static status_t get_cursor_pos(struct device *dev, int *col, int *row)
{
    struct vconsole_data *data = (struct vconsole_data *)dev->data;

    if (data->current_vmode_info.text) {
        return data->conif->get_cursor_pos(data->fbdev, col, row);
    }

    if (col) *col = data->cursor_col;
    if (row) *row = data->cursor_row;

    return STATUS_SUCCESS;
}

static status_t set_cursor_visibility(struct device *dev, int visibility)
{
    struct vconsole_data *data = (struct vconsole_data *)dev->data;

    if (data->current_vmode_info.text) {
        return data->conif->set_cursor_visibility(data->fbdev, visibility);
    }

    data->cursor_visible = visibility;

    flush(dev);

    return STATUS_SUCCESS;
}

static status_t get_cursor_visibility(struct device *dev, int *visibility)
{
    struct vconsole_data *data = (struct vconsole_data *)dev->data;

    if (data->current_vmode_info.text) {
        return data->conif->get_cursor_visibility(data->fbdev, visibility);
    }

    if (visibility) *visibility = data->cursor_visible;

    return STATUS_SUCCESS;
}

static const struct console_interface conif = {
    .get_dimension = get_dimension,
    .get_buffer = get_buffer,
    .invalidate = invalidate,
    .flush = flush,
    .set_cursor_pos = set_cursor_pos,
    .get_cursor_pos = get_cursor_pos,
    .set_cursor_visibility = set_cursor_visibility,
    .get_cursor_visibility = get_cursor_visibility,
    .set_cursor_attr = NULL,
    .get_cursor_attr = NULL,
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

static void vconsole_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"vconsole\"");
    }

    drv->name = "vconsole";
    drv->probe = probe;
    drv->remove = remove;
    drv->get_interface = get_interface;
}

static void video_mode_callback(void *_dev, struct device *fbdev, int mode)
{
    struct device *dev = _dev;
    struct vconsole_data *data = (struct vconsole_data *)dev->data;
    struct console_char_cell *new_char_buffer;
    uint8_t *new_diff_buffer;
    status_t status;

    data->is_switching_mode = 1;

    data->current_vmode = mode;

    status = data->vidif->get_mode_info(fbdev, data->current_vmode, &data->current_vmode_info);
    if (!CHECK_SUCCESS(status)) goto has_error;

    if (data->current_vmode_info.text) {
        LOG_DEBUG("video device is now switching to text mode. freeing buffers...\n");
        if (data->char_buffer) {
            free(data->char_buffer);
            data->char_buffer = NULL;
        }
        if (data->diff_buffer) {
            free(data->diff_buffer);
            data->diff_buffer = NULL;
        }
    } else {
        data->cols = data->current_vmode_info.width / 8;
        data->rows = data->current_vmode_info.height / 16;

        LOG_DEBUG("allocating buffers... cols=%d, rows=%d\n", data->cols, data->rows);
        new_char_buffer =
            realloc(data->char_buffer, data->cols * data->rows * sizeof(*data->char_buffer));
        if (!new_char_buffer) goto has_error;
        data->char_buffer = new_char_buffer;
        memset(data->char_buffer, 0, data->cols * data->rows * sizeof(*data->char_buffer));

        new_diff_buffer = realloc(data->diff_buffer, data->cols * data->rows / 8);
        if (!new_diff_buffer) goto has_error;
        data->diff_buffer = new_diff_buffer;
        memset(data->diff_buffer, 0, data->cols * data->rows / 8);

        invalidate(dev, 0, 0, data->cols - 1, data->rows - 1);
        flush(dev);
    }

    data->cursor_col = 0;
    data->cursor_row = 0;
    data->cursor_prev_col = 0;
    data->cursor_prev_row = 0;

    data->is_switching_mode = 0;

    return;

has_error:
    VlP_Panic(STATUS_UNKNOWN_ERROR, "I think there's no way to recover this");
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
    struct device *fbdev = NULL;
    const struct video_interface *vidif = NULL;
    const struct framebuffer_interface *fbif = NULL;
    const struct console_interface *conif = NULL;
    struct vconsole_data *data = NULL;

    fbdev = parent;
    if (!fbdev) {
        status = STATUS_INVALID_VALUE;
        goto has_error;
    }

    status = fbdev->driver->get_interface(fbdev, "video", (const void **)&vidif);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = fbdev->driver->get_interface(fbdev, "framebuffer", (const void **)&fbif);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = fbdev->driver->get_interface(fbdev, "console", (const void **)&conif);
    if (!CHECK_SUCCESS(status)) {  // optional
        conif = NULL;
    }

    status = VlDev_Create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_GenerateName("console", dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->fbdev = fbdev;
    data->vidif = vidif;
    data->fbif = fbif;
    data->conif = conif;
    data->char_buffer = NULL;
    data->diff_buffer = NULL;
    data->cursor_prev_col = -1;
    data->cursor_prev_row = -1;
    data->cursor_col = 0;
    data->cursor_row = 0;
    data->cursor_visible = 1;
    data->fbdev_callback_id = -1;
    data->is_switching_mode = 0;
    dev->data = data;

    status = vidif->get_mode(fbdev, &data->current_vmode);
    if (!CHECK_SUCCESS(status)) goto has_error;

    video_mode_callback(dev, fbdev, data->current_vmode);

    vidif->add_mode_callback(fbdev, dev, video_mode_callback, &data->fbdev_callback_id);

    if (devout) *devout = dev;

    LOG_DEBUG("initialization success\n");

    return STATUS_SUCCESS;

has_error:
    if (data && data->fbdev_callback_id >= 0) {
        vidif->remove_mode_callback(fbdev, data->fbdev_callback_id);
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
    struct vconsole_data *data = (struct vconsole_data *)dev->data;

    if (data->char_buffer) {
        free(data->char_buffer);
    }

    if (data->diff_buffer) {
        free(data->diff_buffer);
    }

    data->vidif->remove_mode_callback(data->fbdev, data->fbdev_callback_id);

    free(data);

    VlDev_Remove(dev);

    return STATUS_SUCCESS;
}

static status_t get_interface(struct device *dev, const char *name, const void **result)
{
    if (strcmp(name, "console") == 0) {
        if (result) *result = &conif;
        return STATUS_SUCCESS;
    }

    return STATUS_ENTRY_NOT_FOUND;
}

REGISTER_DEVICE_DRIVER(vconsole, vconsole_init)
