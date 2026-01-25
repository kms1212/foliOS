#include <vellum/asm/bios/video.h>

#include <vellum/asm/bios/bioscall.h>

void _pc_bios_video_set_mode(uint8_t mode)
{
    struct bioscall_regs regs = {.a.b.h = 0x00, .a.b.l = mode};

    _pc_bios_call(0x10, &regs);
}

void _pc_bios_video_set_cursor_shape(uint16_t shape)
{
    struct bioscall_regs regs = {.a.b.h = 0x01, .c.w = shape};

    _pc_bios_call(0x10, &regs);
}

void _pc_bios_video_set_cursor_pos(uint8_t page, uint8_t row, uint8_t col)
{
    struct bioscall_regs regs = {
        .a.b.h = 0x02,
        .b.b.h = page,
        .d.b.h = row,
        .d.b.l = col,
    };

    _pc_bios_call(0x10, &regs);
}

void _pc_bios_video_get_cursor_info(uint8_t page, uint16_t *shape, uint8_t *row, uint8_t *col)
{
    struct bioscall_regs regs = {
        .a.b.h = 0x03,
        .b.b.h = page,
    };

    _pc_bios_call(0x10, &regs);

    if (shape) *shape = regs.c.w;
    if (row) *row = regs.d.b.h;
    if (col) *col = regs.d.b.l;
}

void _pc_bios_video_write_tty(uint8_t ch)
{
    struct bioscall_regs regs = {
        .a.b.h = 0x0E,
        .a.b.l = ch,
        .b.b.h = 0x00,
        .b.b.l = 0x0F,
    };

    _pc_bios_call(0x10, &regs);
}

void _pc_bios_video_set_pixel(uint8_t page, uint16_t x, uint16_t y, uint8_t color)
{
    struct bioscall_regs regs = {
        .a.b.h = 0x0C,
        .a.b.l = color,
        .d.b.h = page,
        .c.w = x,
        .d.w = y,
    };

    _pc_bios_call(0x10, &regs);
}

uint8_t _pc_bios_video_get_pixel(uint8_t page, uint16_t x, uint16_t y)
{
    struct bioscall_regs regs = {
        .a.b.h = 0x0D,
        .d.b.h = page,
        .c.w = x,
        .d.w = y,
    };

    _pc_bios_call(0x10, &regs);

    return regs.a.b.l;
}

void _pc_bios_video_get_font_data(uint8_t font_type, const void **data, uint16_t *len)
{
    struct bioscall_regs regs = {
        .a.w = 0x1130,
        .b.b.h = font_type,
    };

    _pc_bios_call(0x10, &regs);

    if (data) {
        *data = (const void *)(((uintptr_t)regs.es.w << 4) + (uintptr_t)regs.bp.w);
    }

    if (len) {
        *len = regs.c.w;
    }
}

status_t _pc_bios_vbe_get_controller_info(struct vbe_controller_info *buf)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F00,
        .es.w = ((uintptr_t)buf >> 4) & 0xFFFF,
        .di.w = (uintptr_t)buf & 0x000F,
    };

    _pc_bios_call(0x10, &regs);

    if (regs.a.b.l != 0x4F) return STATUS_UNSUPPORTED;

    return regs.a.b.h ? STATUS_UNKNOWN_ERROR : STATUS_SUCCESS;
}

status_t _pc_bios_vbe_get_video_mode_info(uint16_t mode, struct vbe_video_mode_info *buf)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F01,
        .c.w = mode,
        .es.w = ((uintptr_t)buf >> 4) & 0xFFFF,
        .di.w = (uintptr_t)buf & 0x000F,
    };

    _pc_bios_call(0x10, &regs);

    if (regs.a.b.l != 0x4F) return STATUS_UNSUPPORTED;

    return regs.a.b.h ? STATUS_UNKNOWN_ERROR : STATUS_SUCCESS;
}

status_t _pc_bios_vbe_set_video_mode(uint16_t mode)
{
    struct bioscall_regs regs = {.a.w = 0x4F02, .b.w = mode};

    _pc_bios_call(0x10, &regs);

    if (regs.a.b.l != 0x4F) return STATUS_UNSUPPORTED;

    return regs.a.b.h ? STATUS_UNKNOWN_ERROR : STATUS_SUCCESS;
}

status_t _pc_bios_vbe_get_video_mode(uint16_t *mode)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F03,
    };

    _pc_bios_call(0x10, &regs);

    if (regs.a.b.l != 0x4F) return STATUS_UNSUPPORTED;
    if (regs.a.b.h) return STATUS_UNKNOWN_ERROR;

    if (mode) *mode = regs.b.w & 0x3FFF;

    return STATUS_SUCCESS;
}

status_t _pc_bios_vbe_set_display_start(uint16_t x, uint16_t y)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F07,
        .b.b.h = 0x00,
        .b.b.l = 0x00,
        .c.w = x,
        .d.w = y,
    };

    if (regs.a.b.l != 0x4F) return STATUS_UNSUPPORTED;

    return regs.a.b.h ? STATUS_UNKNOWN_ERROR : STATUS_SUCCESS;
}

status_t _pc_bios_vbe_set_display_start_at_retrace(uint16_t x, uint16_t y)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F07,
        .b.b.h = 0x00,
        .b.b.l = 0x80,
        .c.w = x,
        .d.w = y,
    };

    if (regs.a.b.l != 0x4F) return STATUS_UNSUPPORTED;

    return regs.a.b.h ? STATUS_UNKNOWN_ERROR : STATUS_SUCCESS;
}

status_t _pc_bios_vbe_schedule_display_start(uint32_t fboffset)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F07,
        .b.b.h = 0x00,
        .b.b.l = 0x02,
        .c.l = fboffset,
    };

    if (regs.a.b.l != 0x4F) return STATUS_UNSUPPORTED;

    return regs.a.b.h ? STATUS_UNKNOWN_ERROR : STATUS_SUCCESS;
}

status_t _pc_bios_vbe_schedule_display_start_at_retrace(uint32_t fboffset)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F07,
        .b.b.h = 0x00,
        .b.b.l = 0x82,
        .c.l = fboffset,
    };

    if (regs.a.b.l != 0x4F) return STATUS_UNSUPPORTED;

    return regs.a.b.h ? STATUS_UNKNOWN_ERROR : STATUS_SUCCESS;
}

status_t _pc_bios_vbe_get_pm_interface(farptr16_t *pmi_table, uint16_t *size)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F0A,
        .b.b.l = 0x00,
    };

    _pc_bios_call(0x10, &regs);

    if (regs.a.b.l != 0x4F) return STATUS_UNSUPPORTED;
    if (regs.a.b.h) return STATUS_UNKNOWN_ERROR;

    if (pmi_table) {
        pmi_table->segment = regs.es.w;
        pmi_table->offset = regs.di.w;
    }
    if (size) {
        *size = regs.c.w;
    }

    return STATUS_SUCCESS;
}

status_t _pc_bios_vbeddc_check_capability(
    uint16_t ctrlr_unit, uint8_t *xfer_time, uint8_t *ddc_level
)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F15,
        .b.b.l = 0x00,
        .c.w = ctrlr_unit,
        .es.w = 0,
        .di.w = 0,
    };

    _pc_bios_call(0x10, &regs);

    if (regs.a.b.l != 0x4F) return STATUS_UNSUPPORTED;
    if (regs.a.b.h) return STATUS_UNKNOWN_ERROR;

    if (xfer_time) {
        *xfer_time = regs.b.b.h;
    }

    if (ddc_level) {
        *ddc_level = regs.b.b.l;
    }

    return STATUS_SUCCESS;
}

status_t _pc_bios_vbeddc_get_edid(uint16_t ctrlr_unit, uint16_t edid_block, struct edid *buf)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F15,
        .b.b.l = 0x01,
        .c.w = ctrlr_unit,
        .d.w = edid_block,
        .es.w = ((uintptr_t)buf >> 4) & 0xFFFF,
        .di.w = (uintptr_t)buf & 0x000F,
    };

    _pc_bios_call(0x10, &regs);

    if (regs.a.b.l != 0x4F) return STATUS_UNSUPPORTED;

    return regs.a.b.h ? STATUS_UNKNOWN_ERROR : STATUS_SUCCESS;
}
