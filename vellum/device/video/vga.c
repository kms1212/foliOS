#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vellum/arch/io.h>

#include <vellum/plat/bios/vbe_pmi.h>
#include <vellum/plat/bios/video.h>
#include <vellum/plat/isr.h>

#include <vellum/device.h>
#include <vellum/encoding/cp437.h>
#include <vellum/interface/console.h>
#include <vellum/interface/framebuffer.h>
#include <vellum/interface/video.h>
#include <vellum/log.h>
#include <vellum/macros.h>
#include <vellum/mm.h>
#include <vellum/panic.h>
#include <vellum/status.h>

#define MODULE_NAME "vga"

#define DIFF_REGION_SIZE 16

#define VGA_CR_M_ADDR 0x03B4
#define VGA_CR_M_DATA 0x03B5
#define VGA_FCR       0x03BA
#define VGA_AR_AD     0x03C0
#define VGA_AR_DR     0x03C1
#define VGA_ISR0      0x03C2
#define VGA_MISCW     0x03C2
#define VGA_SR_ADDR   0x03C4
#define VGA_SR_DATA   0x03C5
#define VGA_PALMASK   0x03C6
#define VGA_DR_STATE  0x03C7
#define VGA_DR_ADDRR  0x03C7
#define VGA_DR_ADDRW  0x03C8
#define VGA_DR_DATA   0x03C9
#define VGA_MISCR     0x03CC
#define VGA_GR_ADDR   0x03CE
#define VGA_GR_DATA   0x03CF
#define VGA_CR_C_ADDR 0x03D4
#define VGA_CR_C_DATA 0x03D5
#define VGA_ISR1      0x03DA

struct callback_list_entry {
    struct callback_list_entry *next;
    int id;
    void *data;
    video_mode_callback_t func;
};

struct vga_data {
    struct console_char_cell *char_buffer;
    uint32_t *frame_buffer;
    uint8_t *diff_buffer;
    int vbe_available;
    int vbe_offers_nonvbe_mode_info;

    int is_switching_mode;
    uint16_t video_mode;
    struct vbe_video_mode_info vbe_mode_info;
    int vga_width, vga_height;
    int mode_set_by_vbe;

    int cursor_col, cursor_row;
    int cursor_visible;
    uint8_t cursor_shape_start, cursor_shape_end;

    struct callback_list_entry *mode_callback_list;

    void (*convert_color)(const struct vbe_video_mode_info *, const uint32_t *, void *, long);
};

static void write_cr(int idx, uint8_t val, int mda)
{
    if (mda) {
        VlA_Out8(VGA_CR_M_ADDR, idx);
        VlA_Out8(VGA_CR_M_DATA, val);
    } else {
        VlA_Out8(VGA_CR_C_ADDR, idx);
        VlA_Out8(VGA_CR_C_DATA, val);
    }
}

static uint8_t read_cr(int idx, int mda)
{
    if (mda) {
        VlA_Out8(VGA_CR_M_ADDR, idx);
        return VlA_In8(VGA_CR_M_DATA);
    } else {
        VlA_Out8(VGA_CR_C_ADDR, idx);
        return VlA_In8(VGA_CR_C_DATA);
    }
}

static void write_ar(int idx, uint8_t val)
{
    VlA_In8(VGA_ISR1);
    uint8_t temp = VlA_In8(VGA_AR_AD);
    VlA_Out8(VGA_AR_AD, idx);
    VlA_Out8(VGA_AR_AD, val);
    VlA_Out8(VGA_AR_AD, temp);
}

static uint8_t read_ar(int idx)
{
    VlA_In8(VGA_ISR1);
    uint8_t temp = VlA_In8(VGA_AR_AD);
    VlA_Out8(VGA_AR_AD, idx);
    uint8_t val = VlA_In8(VGA_AR_DR);
    VlA_In8(VGA_ISR1);
    VlA_Out8(VGA_AR_AD, temp);
    return val;
}

inline static uint32_t get_pixel(struct device *dev, int x, int y)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    return data->frame_buffer[y * data->vbe_mode_info.width + x];
}

inline static void set_region_diff(struct device *dev, int xregion, int yregion)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    data->diff_buffer[yregion * data->vbe_mode_info.width / DIFF_REGION_SIZE + xregion] = 1;
}

inline static void reset_region_diff(struct device *dev, int xregion, int yregion)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    data->diff_buffer[yregion * data->vbe_mode_info.width / DIFF_REGION_SIZE + xregion] = 0;
}

inline static int get_region_diff(struct device *dev, int xregion, int yregion)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    return data->diff_buffer[yregion * data->vbe_mode_info.width / DIFF_REGION_SIZE + xregion];
}

static uint8_t rgb_to_irgb(uint32_t color)
{
    uint8_t r = (color >> 16) & 0xFF, g = (color >> 8) & 0xFF, b = color & 0xFF;

    uint8_t max = r;
    uint8_t min = r;
    if (g > max) max = g;
    if (g < min) min = g;
    if (b > max) max = b;
    if (b < min) min = b;

    uint8_t max_diff = max - min;
    if (max_diff < 0x50) {
        if ((max + min) / 2 < 0x40) {
            return 0x00;
        } else if ((max + min) / 2 <= 0x80) {
            return 0x08;
        } else if ((max + min) / 2 < 0xC0) {
            return 0x07;
        } else {
            return 0x0F;
        }
    }

    return (((max >= 192) ? 1 : 0) << 3) | ((r >= 0x80 ? 1 : 0) << 2) | ((g >= 0x80 ? 1 : 0) << 1) |
        (b >= 0x80 ? 1 : 0);
}

__attribute__((hot)) static void convert_color_rgbx8888(
    const struct vbe_video_mode_info *mode, const uint32_t *orig, void *dest, long count
)
{
    memcpy(dest, orig, count * sizeof(uint32_t));
}

__attribute__((hot)) static void convert_color_rgb888(
    const struct vbe_video_mode_info *mode, const uint32_t *orig, void *dest, long count
)
{
    while (count > 0) {
        if ((uint32_t)dest & 1) {
            *(uint8_t *)dest = *(const uint8_t *)orig;
            *(uint16_t *)((uint8_t *)dest + 1) = *(const uint16_t *)((const uint8_t *)orig + 1);
        } else if (count > 1) {
            *(uint32_t *)dest = *orig;
        } else {
            *(uint16_t *)dest = *(const uint16_t *)orig;
            *((uint8_t *)dest + 2) = *((const uint8_t *)orig + 2);
        }

        dest = (uint8_t *)dest + 3;
        orig++;
        count--;
    }
}

__attribute__((hot)) static void convert_color_rgb565(
    const struct vbe_video_mode_info *mode, const uint32_t *orig, void *dest, long count
)
{
    while (count > 0) {
        *(uint16_t *)dest =
            ((*orig & 0xF80000) >> 8) | ((*orig & 0x00FC00) >> 5) | ((*orig & 0x0000F8) >> 3);

        dest = (uint16_t *)dest + 1;
        orig++;
        count--;
    }
}

__attribute__((hot)) static void convert_color_generic(
    const struct vbe_video_mode_info *mode, const uint32_t *orig, void *dest, long count
)
{
    while (count > 0) {
        uint8_t r = ((*orig >> 16) & 0xFF) >> (8 - mode->red_mask);
        uint8_t g = ((*orig >> 8) & 0xFF) >> (8 - mode->green_mask);
        uint8_t b = (*orig & 0xFF) >> (8 - mode->blue_mask);

        uint32_t color =
            (r << mode->red_position) | (g << mode->green_position) | (b << mode->blue_position);

        switch ((mode->bpp + 7) >> 3) {
        case 1:
            *(uint8_t *)dest = color & 0xFF;
            break;
        case 2:
            *(uint16_t *)dest = color & 0xFFFF;
            break;
        case 3:
            if ((uint32_t)dest & 1) {
                *(uint8_t *)dest = color & 0xFF;
                *(uint16_t *)((uint8_t *)dest + 1) = (color & 0xFFFF00) >> 8;
            } else if (count > 1) {
                *(uint32_t *)dest = color;
            } else {
                *(uint16_t *)dest = color & 0xFFFF;
                *((uint8_t *)dest + 2) = (color & 0xFF0000) >> 16;
            }
            break;
        case 4:
            *(uint32_t *)dest = color;
            break;
        default:
            return;
        }

        dest = (uint8_t *)dest + ((mode->bpp + 7) >> 3);
        orig++;
        count--;
    }
}

static status_t setup_bitmap_buffer(struct device *dev, int width, int height, int bpp)
{
    struct vga_data *data = (struct vga_data *)dev->data;
    status_t status;
    void *new_frame_buffer, *new_diff_buffer;

    if (data->char_buffer) {
        LOG_DEBUG("freeing character buffer...\n");
        free(data->char_buffer);
        data->char_buffer = NULL;
    }

    LOG_DEBUG("(re)allocating frame buffer...\n");
    new_frame_buffer = realloc(data->frame_buffer, width * height * sizeof(*data->frame_buffer));
    if (!new_frame_buffer) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    data->frame_buffer = new_frame_buffer;
    memset(data->frame_buffer, 0, width * height * sizeof(*data->frame_buffer));

    LOG_DEBUG("(re)allocating diff buffer...\n");
    new_diff_buffer =
        realloc(data->diff_buffer, (width / DIFF_REGION_SIZE) * (height / DIFF_REGION_SIZE));
    if (!new_diff_buffer) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    data->diff_buffer = new_diff_buffer;
    memset(data->diff_buffer, 0, (width / DIFF_REGION_SIZE) * (height / DIFF_REGION_SIZE));

    return STATUS_SUCCESS;

has_error:
    VlP_Panic(status, "failed to initialize buffers for video");
}

static status_t setup_text_buffer(struct device *dev, int width, int height)
{
    struct vga_data *data = (struct vga_data *)dev->data;
    status_t status;
    void *new_char_buffer, *new_diff_buffer;

    if (data->frame_buffer) {
        LOG_DEBUG("freeing frame buffer...\n");
        free(data->frame_buffer);
        data->frame_buffer = NULL;
    }

    LOG_DEBUG("(re)allocating character buffer...\n");
    new_char_buffer = realloc(data->char_buffer, width * height * sizeof(*data->char_buffer));
    if (!new_char_buffer) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    data->char_buffer = new_char_buffer;
    memset(data->char_buffer, 0, width * height * sizeof(*data->char_buffer));

    LOG_DEBUG("(re)allocating diff buffer...\n");
    new_diff_buffer = realloc(data->diff_buffer, width * height / 8);
    if (!new_diff_buffer) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    data->diff_buffer = new_diff_buffer;
    memset(data->diff_buffer, 0, width * height / 8);

    return STATUS_SUCCESS;

has_error:
    VlP_Panic(status, "failed to initialize buffers for video");
}

static status_t set_cursor_pos(struct device *dev, int col, int row);

static status_t set_mode_vga(struct device *dev, int mode)
{
    struct vga_data *data = (struct vga_data *)dev->data;
    status_t status;

    switch (mode) {
    case 0x00:
        data->vga_width = 40;
        data->vga_height = 25;
        break;
    case 0x03:
        data->vga_width = 80;
        data->vga_height = 25;
        break;
    default:
        return STATUS_ENTRY_NOT_FOUND;
    }

    data->is_switching_mode = 1;
    data->mode_set_by_vbe = 0;
    data->video_mode = mode;

    status = setup_text_buffer(dev, data->vga_width, data->vga_height);
    if (!CHECK_SUCCESS(status)) return status;

    memset((void *)0xB8000, 0, data->vga_width * data->vga_height * sizeof(uint16_t));
    set_cursor_pos(dev, 0, 0);

    LOG_DEBUG("setting video mode...\n");
    VlBiosP_SetVideoMode(mode);

    write_ar(0x10, read_ar(0x10) & 0xF7);

    data->is_switching_mode = 0;

    return STATUS_SUCCESS;
}

static status_t set_mode_vbe(struct device *dev, int mode)
{
    struct vga_data *data = (struct vga_data *)dev->data;
    status_t status;
    struct vbe_video_mode_info vbe_mode_info;
    size_t hw_frame_size;

    LOG_DEBUG("getting video mode info...\n");
    status = VlBiosP_GetVbeVideoModeInfo(mode, &vbe_mode_info);
    if (!CHECK_SUCCESS(status)) {
        /* try non-VBE modes */
        if (data->vbe_offers_nonvbe_mode_info) {
            return set_mode_vga(dev, mode);
        }

        return status;
    }

    data->is_switching_mode = 1;

    if (vbe_mode_info.memory_model == VBEMM_TEXT) {
        status = setup_text_buffer(dev, vbe_mode_info.width, vbe_mode_info.height);
        if (!CHECK_SUCCESS(status)) goto has_error;

        if (!vbe_mode_info.framebuffer) {
            memset(
                (void *)0xB8000,
                0,
                vbe_mode_info.width * vbe_mode_info.height * sizeof(uint16_t)
            );
        }

        set_cursor_pos(dev, 0, 0);
    } else {
        status =
            setup_bitmap_buffer(dev, vbe_mode_info.width, vbe_mode_info.height, vbe_mode_info.bpp);
        if (!CHECK_SUCCESS(status)) goto has_error;

        if (vbe_mode_info.bpp == 24) {
            data->convert_color = convert_color_rgb888;
        } else if (vbe_mode_info.bpp == 32) {
            data->convert_color = convert_color_rgbx8888;
        } else if (vbe_mode_info.bpp == 16) {
            data->convert_color = convert_color_rgb565;
        } else {
            data->convert_color = convert_color_generic;
        }

        hw_frame_size = vbe_mode_info.pitch * vbe_mode_info.height * vbe_mode_info.bpp / 8;
        mm_map(
            vbe_mode_info.framebuffer / PAGE_SIZE,
            vbe_mode_info.framebuffer / PAGE_SIZE,
            ALIGN_DIV(
                vbe_mode_info.framebuffer % PAGE_SIZE +
                    (vbe_mode_info.lin_num_image_pages + 1) * hw_frame_size,
                PAGE_SIZE
            ),
            PF_WTCACHE
        );
        memset(
            (void *)vbe_mode_info.framebuffer,
            0,
            (vbe_mode_info.lin_num_image_pages + 1) * hw_frame_size
        );
    }

    memcpy(&data->vbe_mode_info, &vbe_mode_info, sizeof(data->vbe_mode_info));

    data->video_mode = mode;
    data->mode_set_by_vbe = 1;

    LOG_DEBUG("setting video mode...\n");
    status = VlBiosP_SetVbeVideoMode(mode);
    if (!CHECK_SUCCESS(status)) goto has_error;

    data->is_switching_mode = 0;

    return STATUS_SUCCESS;

has_error:
    VlP_Panic(status, "failed to change video mode");
}

static status_t set_mode(struct device *dev, int mode)
{
    struct vga_data *data = (struct vga_data *)dev->data;
    status_t status;

    status = data->vbe_available ? set_mode_vbe(dev, mode) : set_mode_vga(dev, mode);

    VlIntP_Unmask(0x20);
    VlIntP_Unmask(0x28);

    for (struct callback_list_entry *current = data->mode_callback_list; current;
         current = current->next) {
        current->func(current->data, dev, mode);
    }

    return status;
}

static status_t get_mode(struct device *dev, int *mode)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    if (mode) *mode = data->video_mode;

    return STATUS_SUCCESS;
}

static status_t add_mode_callback(
    struct device *dev, void *cb_data, video_mode_callback_t callback, int *
)
{
    struct vga_data *data = (struct vga_data *)dev->data;
    struct callback_list_entry *entry;
    int new_id = 0;

    entry = malloc(sizeof(*entry));
    if (!entry) return STATUS_UNKNOWN_ERROR;

    entry->data = cb_data;
    entry->func = callback;

    if (!data->mode_callback_list) {
        data->mode_callback_list = entry;
    } else {
        struct callback_list_entry *current = data->mode_callback_list;
        for (; current->next; current = current->next) {
            if (new_id < current->id) {
                new_id = current->id + 1;
            }
        }

        current->next = entry;
    }

    entry->id = new_id;

    LOG_DEBUG("video mode callback added\n");

    return STATUS_SUCCESS;
}

static void remove_mode_callback(struct device *dev, int id)
{
    struct vga_data *data = (struct vga_data *)dev->data;
    struct callback_list_entry *prev_entry = NULL;
    struct callback_list_entry *next_entry;

    if (!data->mode_callback_list) return;

    for (struct callback_list_entry *current = data->mode_callback_list; current->next;
         current = current->next) {
        if (current->next->id == id) {
            prev_entry = current;
            break;
        }
    }
    if (!prev_entry) return;

    next_entry = prev_entry->next->next;

    free(prev_entry->next);

    prev_entry->next = next_entry;

    LOG_DEBUG("video mode callback removed\n");
}

static status_t get_mode_info_vga(struct device *dev, int mode, struct video_mode_info *mode_info)
{
    if (mode < 0) mode = 0x00;

    switch (mode) {
    case 0x00:
        mode_info->current_mode = 0x00;
        mode_info->next_mode = 0x03;
        mode_info->text = 1;
        mode_info->width = 40;
        mode_info->height = 25;
        break;
    case 0x03:
        mode_info->current_mode = 0x03;
        mode_info->next_mode = -1;
        mode_info->text = 1;
        mode_info->width = 80;
        mode_info->height = 25;
        break;
    default:
        return STATUS_ENTRY_NOT_FOUND;
    }

    return STATUS_SUCCESS;
}

static status_t get_mode_info_vbe(struct device *dev, int mode, struct video_mode_info *mode_info)
{
    status_t status;
    struct vga_data *data = (struct vga_data *)dev->data;
    struct vbe_controller_info vbe_info;
    uint16_t *mode_list;
    struct vbe_video_mode_info vbe_mode_info;
    int finding_next_mode;

    status = VlBiosP_GetVbeControllerInfo(&vbe_info);
    if (!CHECK_SUCCESS(status)) return status;

    mode_list = (uint16_t *)FARPTR16_TO_VPTR(vbe_info.video_modes);

    finding_next_mode = 0;
    for (int i = 0; mode_list[i] != 0xFFFF; i++) {
        if (mode >= 0 && !finding_next_mode && mode_list[i] != mode) continue;

        status = VlBiosP_GetVbeVideoModeInfo(mode_list[i], &vbe_mode_info);
        if (!CHECK_SUCCESS(status)) return status;

        if (vbe_mode_info.memory_model != VBEMM_DIRECT &&
            vbe_mode_info.memory_model != VBEMM_TEXT) {
            continue;
        }

        if (finding_next_mode) {
            if (mode_info) {
                mode_info->next_mode = mode_list[i];
            }

            return STATUS_SUCCESS;
        } else {
            if (mode_info) {
                mode_info->current_mode = mode_list[i];
                mode_info->next_mode = -1;
                mode_info->text = vbe_mode_info.memory_model == VBEMM_TEXT;
                mode_info->width = vbe_mode_info.width;
                mode_info->height = vbe_mode_info.height;
                mode_info->bpp = vbe_mode_info.bpp;
            }

            finding_next_mode = 1;
        }
    }

    if (data->vbe_offers_nonvbe_mode_info) {
        return STATUS_SUCCESS;
    }

    if (finding_next_mode) {
        if (mode_info) {
            mode_info->next_mode = 0x00;
        }

        return STATUS_SUCCESS;
    }

    return get_mode_info_vga(dev, mode, mode_info);
}

static status_t get_mode_info(struct device *dev, int mode, struct video_mode_info *mode_info)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    return data->vbe_available ? get_mode_info_vbe(dev, mode, mode_info)
                               : get_mode_info_vga(dev, mode, mode_info);
}

static status_t get_hw_mode_info_vbe(
    struct device *dev, int mode, struct video_hw_mode_info *hwmode
)
{
    struct vga_data *data = (struct vga_data *)dev->data;
    status_t status;
    struct vbe_video_mode_info vbe_mode_info;

    if (!data->vbe_offers_nonvbe_mode_info && mode < 0x100) return STATUS_CONFLICTING_STATE;

    status = VlBiosP_GetVbeVideoModeInfo(mode, &vbe_mode_info);
    if (!CHECK_SUCCESS(status)) return status;

    hwmode->width = vbe_mode_info.width;
    hwmode->height = vbe_mode_info.height;
    hwmode->bpp = vbe_mode_info.bpp;
    switch (vbe_mode_info.memory_model) {
    case VBEMM_DIRECT:
        hwmode->memory_model = VMM_DIRECT;
        hwmode->framebuffer = (void *)vbe_mode_info.framebuffer;
        hwmode->pitch = vbe_mode_info.pitch;
        hwmode->rmask = vbe_mode_info.red_mask;
        hwmode->rpos = vbe_mode_info.red_position;
        hwmode->gmask = vbe_mode_info.green_mask;
        hwmode->gpos = vbe_mode_info.green_position;
        hwmode->bmask = vbe_mode_info.blue_mask;
        hwmode->bpos = vbe_mode_info.blue_position;
        break;
    case VBEMM_TEXT:
        hwmode->memory_model = VMM_TEXT;
        hwmode->framebuffer = (void *)0xB8000;
        hwmode->pitch = (int)(hwmode->width * sizeof(uint16_t));
        break;
    default:
        return 1;
    }

    return STATUS_SUCCESS;
}

static status_t get_hw_mode_info_vga(
    struct device *dev, int mode, struct video_hw_mode_info *hwmode
)
{
    if (mode >= 0x100) return STATUS_CONFLICTING_STATE;

    switch (mode) {
    case 0x00:
        hwmode->memory_model = VMM_TEXT;
        hwmode->framebuffer = (void *)0xB8000;
        hwmode->width = 40;
        hwmode->pitch = 80;
        hwmode->height = 25;
        break;
    case 0x03:
        hwmode->memory_model = VMM_TEXT;
        hwmode->framebuffer = (void *)0xB8000;
        hwmode->width = 80;
        hwmode->pitch = 160;
        hwmode->height = 25;
        break;
    default:
        return STATUS_ENTRY_NOT_FOUND;
    }

    return STATUS_SUCCESS;
}

static status_t get_hw_mode_info(struct device *dev, int mode, struct video_hw_mode_info *hwmode)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    if (!data->vbe_available) return STATUS_NOT_SUPPORTED;

    if (!data->vbe_offers_nonvbe_mode_info && mode < 0x100) {
        return get_hw_mode_info_vga(dev, mode, hwmode);
    }

    return get_hw_mode_info_vbe(dev, mode, hwmode);
}

static const struct video_interface vidif = {
    .set_mode = set_mode,
    .get_mode = get_mode,
    .add_mode_callback = add_mode_callback,
    .remove_mode_callback = remove_mode_callback,
    .get_mode_info = get_mode_info,
    .get_hw_mode_info = get_hw_mode_info,
};

static status_t get_framebuffer_vbe(struct device *dev, void **framebuffer)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    if (framebuffer) *framebuffer = data->frame_buffer;

    return STATUS_SUCCESS;
}

static status_t get_framebuffer(struct device *dev, void **framebuffer)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    if (!data->vbe_available) return STATUS_NOT_SUPPORTED;

    return get_framebuffer_vbe(dev, framebuffer);
}

static status_t fb_invalidate_vbe(struct device *dev, int x0, int y0, int x1, int y1)
{
    for (int yr = y0 / DIFF_REGION_SIZE; yr < (y1 + DIFF_REGION_SIZE - 1) / DIFF_REGION_SIZE;
         yr++) {
        for (int xr = x0 / DIFF_REGION_SIZE; xr < (x1 + DIFF_REGION_SIZE - 1) / DIFF_REGION_SIZE;
             xr++) {
            set_region_diff(dev, xr, yr);
        }
    }

    return STATUS_SUCCESS;
}

static status_t fb_invalidate(struct device *dev, int x0, int y0, int x1, int y1)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    if (data->is_switching_mode) return STATUS_CONFLICTING_STATE;

    if (!data->vbe_available) return STATUS_NOT_SUPPORTED;

    return fb_invalidate_vbe(dev, x0, y0, x1, y1);
}

static int get_diff_chunk_vbe(struct device *dev, int xr, int yr)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    int modified_chunk = get_region_diff(dev, xr, yr);
    int chunk_size = 1;

    while (++xr < data->vbe_mode_info.width / DIFF_REGION_SIZE) {
        if (modified_chunk != get_region_diff(dev, xr, yr)) break;
        chunk_size++;
    }

    return modified_chunk ? chunk_size : -chunk_size;
}

static int get_diff_chunk(struct device *dev, int xr, int yr)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    if (!data->vbe_available) return STATUS_NOT_SUPPORTED;

    return get_diff_chunk_vbe(dev, xr, yr);
}

static status_t fb_flush_vbe(struct device *dev)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    for (int yr = 0; yr < data->vbe_mode_info.height / DIFF_REGION_SIZE; yr++) {
        int xr = 0;
        while (xr < data->vbe_mode_info.width / DIFF_REGION_SIZE) {
            int chunk_size = get_diff_chunk(dev, xr, yr);
            if (chunk_size < 0) {
                xr += -chunk_size;
                continue;
            }

            for (int i = 0; i < chunk_size; i++) {
                reset_region_diff(dev, xr + i, yr);
            }

            for (int y = yr * DIFF_REGION_SIZE; y < (yr + 1) * DIFF_REGION_SIZE; y++) {
                long fb_offs = (y)*data->vbe_mode_info.pitch +
                    xr * DIFF_REGION_SIZE * data->vbe_mode_info.bpp / 8;

                data->convert_color(
                    &data->vbe_mode_info,
                    &data->frame_buffer[y * data->vbe_mode_info.width + xr * DIFF_REGION_SIZE],
                    (void *)(data->vbe_mode_info.framebuffer + fb_offs),
                    chunk_size * DIFF_REGION_SIZE
                );
            }

            xr += chunk_size;
        }
    }

    return STATUS_SUCCESS;
}

static status_t fb_flush(struct device *dev)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    if (data->is_switching_mode) return STATUS_CONFLICTING_STATE;

    if (!data->vbe_available) return STATUS_NOT_SUPPORTED;

    return fb_flush_vbe(dev);
}

static const struct framebuffer_interface fbif = {
    .get_framebuffer = get_framebuffer,
    .invalidate = fb_invalidate,
    .flush = fb_flush,
};

static status_t get_dimension_vbe(struct device *dev, int *width, int *height)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    if (data->vbe_mode_info.memory_model != VBEMM_TEXT) {
        return STATUS_CONFLICTING_STATE;
    }

    if (width) *width = data->vbe_mode_info.width;
    if (height) *height = data->vbe_mode_info.height;

    return STATUS_SUCCESS;
}

static status_t get_dimension_vga(struct device *dev, int *width, int *height)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    if (data->video_mode > 0xFF) return STATUS_CONFLICTING_STATE;

    if (width) *width = data->vga_width;
    if (height) *height = data->vga_height;

    return STATUS_SUCCESS;
}

static status_t get_dimension(struct device *dev, int *width, int *height)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    return data->mode_set_by_vbe ? get_dimension_vbe(dev, width, height)
                                 : get_dimension_vga(dev, width, height);
}

static status_t get_buffer(struct device *dev, struct console_char_cell **buf)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    if (buf) *buf = data->char_buffer;

    return STATUS_SUCCESS;
}

static status_t con_invalidate(struct device *dev, int x0, int y0, int x1, int y1)
{
    struct vga_data *data = (struct vga_data *)dev->data;
    status_t status;
    int width;

    if (data->is_switching_mode) return STATUS_CONFLICTING_STATE;

    status = get_dimension(dev, &width, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            data->diff_buffer[(y * width + x) / 8] |= 1 << ((y * width + x) % 8);
        }
    }

    return STATUS_SUCCESS;
}

static status_t con_flush(struct device *dev)
{
    struct vga_data *data = (struct vga_data *)dev->data;
    status_t status;
    int width, height;
    const struct console_char_cell *src;
    uint8_t cp437_char;
    uint16_t cell;

    if (data->is_switching_mode) return STATUS_CONFLICTING_STATE;

    status = get_dimension(dev, &width, &height);
    if (!CHECK_SUCCESS(status)) return status;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (!(data->diff_buffer[(y * width + x) / 8] & (1 << ((y * width + x) % 8)))) {
                continue;
            }

            src = &data->char_buffer[y * width + x];
            status = VlEnc_Utf32ToCp437(src->codepoint, &cp437_char);
            if (!CHECK_SUCCESS(status)) {
                cp437_char = '?';
            }
            cell = cp437_char;
            cell |=
                (rgb_to_irgb(src->attr.text_reversed ? src->attr.bg_color : src->attr.fg_color) &
                 0xF)
                << 8;
            cell |=
                (rgb_to_irgb(src->attr.text_reversed ? src->attr.fg_color : src->attr.bg_color) &
                 0xF)
                << 12;
            if (src->attr.text_dim) {
                cell &= 0x77FF;
            }
            ((uint16_t *)0xB8000)[y * width + x] =
                cell;  // NOLINT(clang-analyzer-core.FixedAddressDereference)
            data->diff_buffer[(y * width + x) / 8] &= ~(1 << ((y * width + x) % 8));
        }
    }

    return STATUS_SUCCESS;
}

static status_t set_cursor_pos(struct device *dev, int col, int row)
{
    status_t status;
    struct vga_data *data = (struct vga_data *)dev->data;
    int width, pos;

    status = get_dimension(dev, &width, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    if (col < 0) {
        col = data->cursor_col;
    }
    if (row < 0) {
        row = data->cursor_row;
    }

    pos = row * width + col;

    VlA_Out8(0x03D4, 0x0F);
    VlA_Out8(0x03D5, pos & 0xFF);
    VlA_Out8(0x03D4, 0x0E);
    VlA_Out8(0x03D5, (pos >> 8) & 0xFF);

    data->cursor_col = col;
    data->cursor_row = row;

    return STATUS_SUCCESS;
}

static status_t get_cursor_pos(struct device *dev, int *col, int *row)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    if (col) *col = data->cursor_col;
    if (row) *row = data->cursor_row;

    return STATUS_SUCCESS;
}

static status_t set_cursor_visibility(struct device *dev, int visibility)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    data->cursor_visible = visibility;

    if (visibility) {
        write_cr(0x0A, (read_cr(0x0A, 0) & 0xC0) | data->cursor_shape_start, 0);
        write_cr(0x0B, (read_cr(0x0B, 0) & 0xE0) | data->cursor_shape_end, 0);
    } else {
        write_cr(0x0A, 0x20, 0);
    }

    return STATUS_SUCCESS;
}

static status_t get_cursor_visibility(struct device *dev, int *visibility)
{
    struct vga_data *data = (struct vga_data *)dev->data;

    if (visibility) *visibility = data->cursor_visible;

    return STATUS_SUCCESS;
}

static const struct console_interface conif = {
    .get_dimension = get_dimension,
    .get_buffer = get_buffer,
    .invalidate = con_invalidate,
    .flush = con_flush,
    .set_cursor_pos = set_cursor_pos,
    .get_cursor_pos = get_cursor_pos,
    .set_cursor_attr = NULL,
    .get_cursor_attr = NULL,
    .set_cursor_visibility = set_cursor_visibility,
    .get_cursor_visibility = get_cursor_visibility,
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

static void vga_init(void)
{
    status_t status;
    struct device_driver *drv;

    status = VlDev_CreateDriver(&drv);
    if (!CHECK_SUCCESS(status)) {
        VlP_Panic(status, "cannot register device driver \"vga\"");
    }

    drv->name = "vga";
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
    struct vbe_controller_info vbe_info;
    struct device *dev = NULL;
    struct vga_data *data = NULL;
    uint16_t vbe_vmode;
    uint16_t *mode_list;

    status = VlDev_Create(&dev, drv, parent);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = VlDev_GenerateName("video", dev->name, sizeof(dev->name));
    if (!CHECK_SUCCESS(status)) goto has_error;

    data = malloc(sizeof(*data));
    if (!data) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    data->char_buffer = NULL;
    data->frame_buffer = NULL;
    data->diff_buffer = NULL;
    data->mode_callback_list = NULL;
    data->vbe_offers_nonvbe_mode_info = 0;
    data->cursor_col = data->cursor_row = 0;
    data->cursor_visible = 1;
    data->cursor_shape_start = 14;
    data->cursor_shape_end = 15;
    data->mode_set_by_vbe = 0;
    dev->data = data;

    LOG_DEBUG("checking wheter VBE is supported...\n");
    status = VlBiosP_GetVbeControllerInfo(&vbe_info);
    data->vbe_available = CHECK_SUCCESS(status);
    if (data->vbe_available) {
        LOG_DEBUG("VBE supported\n");
    } else {
        LOG_DEBUG("VBE not supported\n");
    }

    if (data->vbe_available) {
        LOG_DEBUG("checking whether VBE offers non-VBE video modes...\n");
        status = VlBiosP_GetVbeControllerInfo(&vbe_info);
        if (!CHECK_SUCCESS(status)) return status;

        mode_list = (uint16_t *)FARPTR16_TO_VPTR(vbe_info.video_modes);
        for (int i = 0; mode_list[i] != 0xFFFF; i++) {
            if (mode_list[i] < 0x100) {
                data->vbe_offers_nonvbe_mode_info = 1;
            }
        }

        LOG_DEBUG("getting current video mode...\n");
        status = VlBiosP_GetVbeVideoMode(&vbe_vmode);
        if (!CHECK_SUCCESS(status)) goto has_error;

        if (vbe_vmode >= 0x100) {
            LOG_DEBUG("getting current video mode information...\n");
            status = VlBiosP_GetVbeVideoModeInfo(vbe_vmode, &data->vbe_mode_info);
            if (!CHECK_SUCCESS(status)) goto has_error;

            LOG_DEBUG("setting up buffers...\n");
            switch (data->vbe_mode_info.memory_model) {
            case VBEMM_DIRECT:
                status = setup_bitmap_buffer(
                    dev,
                    data->vbe_mode_info.width,
                    data->vbe_mode_info.height,
                    data->vbe_mode_info.bpp
                );
                if (!CHECK_SUCCESS(status)) goto has_error;
                break;
            case VBEMM_TEXT:
                status =
                    setup_text_buffer(dev, data->vbe_mode_info.width, data->vbe_mode_info.height);
                if (!CHECK_SUCCESS(status)) goto has_error;
                break;
            default:
                status = STATUS_NOT_SUPPORTED;
                goto has_error;
            }
        } else {
            LOG_DEBUG("falling back to default video mode...\n");
            status = set_mode(dev, 0x03);
            if (!CHECK_SUCCESS(status)) goto has_error;
        }
    } else {
        LOG_DEBUG("falling back to default video mode...\n");
        status = set_mode(dev, 0x03);
        if (!CHECK_SUCCESS(status)) goto has_error;
    }

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
    struct vga_data *data = (struct vga_data *)dev->data;

    if (data->char_buffer) {
        free(data->char_buffer);
    }

    if (data->frame_buffer) {
        free(data->frame_buffer);
    }

    if (data->diff_buffer) {
        free(data->diff_buffer);
    }

    free(data);

    VlDev_Remove(dev);

    return STATUS_SUCCESS;
}

static status_t get_interface(struct device *dev, const char *name, const void **result)
{
    if (strcmp(name, "framebuffer") == 0) {
        if (result) *result = &fbif;
        return STATUS_SUCCESS;
    } else if (strcmp(name, "console") == 0) {
        if (result) *result = &conif;
        return STATUS_SUCCESS;
    } else if (strcmp(name, "video") == 0) {
        if (result) *result = &vidif;
        return STATUS_SUCCESS;
    }

    return STATUS_ENTRY_NOT_FOUND;
}

REGISTER_DEVICE_DRIVER(vga, vga_init)
