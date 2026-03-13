#include <vellum/plat/bios/video.h>

#include <vellum/plat/bios/bioscall.h>

void VlBiosP_SetVideoMode(uint8_t mode)
{
    struct bioscall_regs regs = {.a.b.h = 0x00, .a.b.l = mode};

    VlBiosP_Call(0x10, &regs);
}

void VlBiosP_SetVideoCursorShape(uint16_t shape)
{
    struct bioscall_regs regs = {.a.b.h = 0x01, .c.w = shape};

    VlBiosP_Call(0x10, &regs);
}

void VlBiosP_SetVideoCursorPos(uint8_t page, uint8_t row, uint8_t col)
{
    struct bioscall_regs regs = {
        .a.b.h = 0x02,
        .b.b.h = page,
        .d.b.h = row,
        .d.b.l = col,
    };

    VlBiosP_Call(0x10, &regs);
}

void VlBiosP_GetVideoCursorInfo(uint8_t page, uint16_t *shape, uint8_t *row, uint8_t *col)
{
    struct bioscall_regs regs = {
        .a.b.h = 0x03,
        .b.b.h = page,
    };

    VlBiosP_Call(0x10, &regs);

    if (shape) *shape = regs.c.w;
    if (row) *row = regs.d.b.h;
    if (col) *col = regs.d.b.l;
}

void VlBiosP_WriteVideoTty(uint8_t ch)
{
    struct bioscall_regs regs = {
        .a.b.h = 0x0E,
        .a.b.l = ch,
        .b.b.h = 0x00,
        .b.b.l = 0x0F,
    };

    VlBiosP_Call(0x10, &regs);
}

void VlBiosP_SetVideoPixel(uint8_t page, uint16_t x, uint16_t y, uint8_t color)
{
    struct bioscall_regs regs = {
        .a.b.h = 0x0C,
        .a.b.l = color,
        .d.b.h = page,
        .c.w = x,
        .d.w = y,
    };

    VlBiosP_Call(0x10, &regs);
}

uint8_t VlBiosP_GetVideoPixel(uint8_t page, uint16_t x, uint16_t y)
{
    struct bioscall_regs regs = {
        .a.b.h = 0x0D,
        .d.b.h = page,
        .c.w = x,
        .d.w = y,
    };

    VlBiosP_Call(0x10, &regs);

    return regs.a.b.l;
}

void VlBiosP_GetVideoFontData(uint8_t font_type, const void **data, uint16_t *len)
{
    struct bioscall_regs regs = {
        .a.w = 0x1130,
        .b.b.h = font_type,
    };

    VlBiosP_Call(0x10, &regs);

    if (data) {
        *data = (const void *)(((uintptr_t)regs.es.w << 4) + (uintptr_t)regs.bp.w);
    }

    if (len) {
        *len = regs.c.w;
    }
}

status_t VlBiosP_GetVbeControllerInfo(struct vbe_controller_info *buf)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F00,
        .es.w = ((uintptr_t)buf >> 4) & 0xFFFF,
        .di.w = (uintptr_t)buf & 0x000F,
    };

    VlBiosP_Call(0x10, &regs);

    if (regs.a.b.l != 0x4F) return STATUS_NOT_SUPPORTED;

    return regs.a.b.h ? STATUS_UNKNOWN_ERROR : STATUS_SUCCESS;
}

status_t VlBiosP_GetVbeVideoModeInfo(uint16_t mode, struct vbe_video_mode_info *buf)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F01,
        .c.w = mode,
        .es.w = ((uintptr_t)buf >> 4) & 0xFFFF,
        .di.w = (uintptr_t)buf & 0x000F,
    };

    VlBiosP_Call(0x10, &regs);

    if (regs.a.b.l != 0x4F) return STATUS_NOT_SUPPORTED;

    return regs.a.b.h ? STATUS_UNKNOWN_ERROR : STATUS_SUCCESS;
}

status_t VlBiosP_SetVbeVideoMode(uint16_t mode)
{
    struct bioscall_regs regs = {.a.w = 0x4F02, .b.w = mode};

    VlBiosP_Call(0x10, &regs);

    if (regs.a.b.l != 0x4F) return STATUS_NOT_SUPPORTED;

    return regs.a.b.h ? STATUS_UNKNOWN_ERROR : STATUS_SUCCESS;
}

status_t VlBiosP_GetVbeVideoMode(uint16_t *mode)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F03,
    };

    VlBiosP_Call(0x10, &regs);

    if (regs.a.b.l != 0x4F) return STATUS_NOT_SUPPORTED;
    if (regs.a.b.h) return STATUS_UNKNOWN_ERROR;

    if (mode) *mode = regs.b.w & 0x3FFF;

    return STATUS_SUCCESS;
}

status_t VlBiosP_SetVbeDisplayStart(uint16_t x, uint16_t y)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F07,
        .b.b.h = 0x00,
        .b.b.l = 0x00,
        .c.w = x,
        .d.w = y,
    };

    if (regs.a.b.l != 0x4F) return STATUS_NOT_SUPPORTED;

    return regs.a.b.h ? STATUS_UNKNOWN_ERROR : STATUS_SUCCESS;
}

status_t VlBiosP_SetVbeDisplayStartAtVsync(uint16_t x, uint16_t y)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F07,
        .b.b.h = 0x00,
        .b.b.l = 0x80,
        .c.w = x,
        .d.w = y,
    };

    if (regs.a.b.l != 0x4F) return STATUS_NOT_SUPPORTED;

    return regs.a.b.h ? STATUS_UNKNOWN_ERROR : STATUS_SUCCESS;
}

status_t VlBiosP_ScheduleVbeDisplayStart(uint32_t fboffset)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F07,
        .b.b.h = 0x00,
        .b.b.l = 0x02,
        .c.l = fboffset,
    };

    if (regs.a.b.l != 0x4F) return STATUS_NOT_SUPPORTED;

    return regs.a.b.h ? STATUS_UNKNOWN_ERROR : STATUS_SUCCESS;
}

status_t VlBiosP_ScheduleVbeDisplayStartAtVsync(uint32_t fboffset)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F07,
        .b.b.h = 0x00,
        .b.b.l = 0x82,
        .c.l = fboffset,
    };

    if (regs.a.b.l != 0x4F) return STATUS_NOT_SUPPORTED;

    return regs.a.b.h ? STATUS_UNKNOWN_ERROR : STATUS_SUCCESS;
}

status_t VlBiosP_GetVbePmiTable(struct VlA_FarPtr16 *pmi_table, uint16_t *size)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F0A,
        .b.b.l = 0x00,
    };

    VlBiosP_Call(0x10, &regs);

    if (regs.a.b.l != 0x4F) return STATUS_NOT_SUPPORTED;
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

status_t VlBiosP_CheckVbeDdcCapability(uint16_t ctrlr_unit, uint8_t *xfer_time, uint8_t *ddc_level)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F15,
        .b.b.l = 0x00,
        .c.w = ctrlr_unit,
        .es.w = 0,
        .di.w = 0,
    };

    VlBiosP_Call(0x10, &regs);

    if (regs.a.b.l != 0x4F) return STATUS_NOT_SUPPORTED;
    if (regs.a.b.h) return STATUS_UNKNOWN_ERROR;

    if (xfer_time) {
        *xfer_time = regs.b.b.h;
    }

    if (ddc_level) {
        *ddc_level = regs.b.b.l;
    }

    return STATUS_SUCCESS;
}

status_t VlBiosP_GetVbeDdcEdid(uint16_t ctrlr_unit, uint16_t edid_block, struct edid *buf)
{
    struct bioscall_regs regs = {
        .a.w = 0x4F15,
        .b.b.l = 0x01,
        .c.w = ctrlr_unit,
        .d.w = edid_block,
        .es.w = ((uintptr_t)buf >> 4) & 0xFFFF,
        .di.w = (uintptr_t)buf & 0x000F,
    };

    VlBiosP_Call(0x10, &regs);

    if (regs.a.b.l != 0x4F) return STATUS_NOT_SUPPORTED;

    return regs.a.b.h ? STATUS_UNKNOWN_ERROR : STATUS_SUCCESS;
}
