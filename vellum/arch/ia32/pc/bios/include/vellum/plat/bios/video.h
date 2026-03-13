#ifndef __VELLUM_ASM_BIOS_VIDEO_H__
#define __VELLUM_ASM_BIOS_VIDEO_H__

#include <stdint.h>

#include <vellum/arch/farptr.h>

#include <vellum/compiler.h>
#include <vellum/status.h>

void VlBiosP_SetVideoMode(uint8_t mode);

void VlBiosP_SetVideoCursorShape(uint16_t shape);

void VlBiosP_SetVideoCursorPos(uint8_t page, uint8_t row, uint8_t col);

void VlBiosP_GetVideoCursorInfo(uint8_t page, uint16_t *shape, uint8_t *row, uint8_t *col);

void VlBiosP_ScrollVideoUp(
    uint8_t amount, uint8_t left, uint8_t right, uint8_t top, uint8_t bottom, uint8_t attr
);

void VlBiosP_ScrollVideoDown(
    uint8_t amount, uint8_t left, uint8_t right, uint8_t top, uint8_t bottom, uint8_t attr
);

void VlBiosP_GetVideoCharAttr(uint8_t *ch, uint8_t *attr);

void VlBiosP_SetVideoCharAttr(uint8_t ch, uint8_t attr, uint16_t count);

void VlBiosP_WriteVideoChar(uint8_t ch, uint16_t count);

void VlBiosP_WriteVideoTty(uint8_t ch);

void VlBiosP_WriteVideoString(
    uint8_t mode, uint8_t attr, uint8_t row, uint8_t col, const void *str, uint16_t len
);

void VlBiosP_SetVideoPixel(uint8_t page, uint16_t x, uint16_t y, uint8_t color);

uint8_t VlBiosP_GetVideoPixel(uint8_t page, uint16_t x, uint16_t y);

void VlBiosP_GetVideoFontData(uint8_t font_type, const void **data, uint16_t *len);

#define VBE_CTRL_SIGNATURE_VBE1 "VESA"
#define VBE_CTRL_SIGNATURE_VBE2 "VBE2"

struct vbe_palette_entry {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t reserved;
} __packed;

struct vbe_crtc_info_block {
    uint16_t htotal;
    uint16_t hsync_start;
    uint16_t hsync_end;
    uint16_t vtotal;
    uint16_t vsync_start;
    uint16_t vsync_end;
    uint8_t flags;
    uint32_t pixel_clock;
    uint16_t refresh_rate;
    uint8_t reserved[40];
} __packed;

struct vbe_pm_interface {
    uint16_t set_window;
    uint16_t set_display_start;
    uint16_t set_primary_palette_data;
    uint16_t port_mem_locations; /* refer to VBE 3.0 spec page 57 */
    uint8_t additional_data[];
} __packed;

struct vbe_controller_info {
    /* VBE 1.0 */
    char signature[4];
    uint16_t vbe_version;
    struct VlA_FarPtr16 oem_string;
    uint32_t capabilities;
    struct VlA_FarPtr16 video_modes;
    uint16_t total_memory;

    /* VBE 2.0 */
    uint16_t oem_software_rev;
    struct VlA_FarPtr16 oem_vendor_name_ptr;
    struct VlA_FarPtr16 oem_product_name_ptr;
    struct VlA_FarPtr16 oem_product_rev_ptr;

    uint8_t reserved[222];

    uint8_t oem_data[256];

} __packed;

enum vbe_memory_model {
    VBEMM_TEXT = 0,
    VBEMM_CGA,
    VBEMM_HERCULES,
    VBEMM_PLANAR,
    VBEMM_PACKED,
    VBEMM_NON_CHAIN,
    VBEMM_DIRECT,
    VBEMM_YUV,
};

struct vbe_video_mode_info {
    /* VBE 1.0 */
    uint16_t attributes;
    uint8_t window_a;
    uint8_t window_b;
    uint16_t granularity;
    uint16_t window_size;
    uint16_t segment_a;
    uint16_t segment_b;
    uint32_t win_func_ptr;
    uint16_t pitch;

    /* VBE 1.2 */
    uint16_t width;
    uint16_t height;
    uint8_t w_char;
    uint8_t y_char;
    uint8_t planes;
    uint8_t bpp;
    uint8_t banks;
    uint8_t memory_model;
    uint8_t bank_size;
    uint8_t image_pages;
    uint8_t reserved0;

    uint8_t red_mask;
    uint8_t red_position;
    uint8_t green_mask;
    uint8_t green_position;
    uint8_t blue_mask;
    uint8_t blue_position;
    uint8_t reserved_mask;
    uint8_t reserved_position;
    uint8_t direct_color_attributes;

    /* VBE 2.0 */
    uint32_t framebuffer;
    uint32_t reserved1;
    uint16_t reserved2;

    /* VBE 3.0 */
    uint16_t lin_bytes_per_scan_line;
    uint8_t bnk_num_image_pages;
    uint8_t lin_num_image_pages;
    uint8_t lin_red_mask;
    uint8_t lin_red_position;
    uint8_t lin_green_mask;
    uint8_t lin_green_position;
    uint8_t lin_blue_mask;
    uint8_t lin_blue_position;
    uint8_t lin_reserved_mask;
    uint8_t lin_reserved_position;
    uint32_t max_pixel_clock;

    uint8_t reserved3[189];
} __packed;

struct edid_chroma_info {
    uint8_t green_red_xy;
    uint8_t white_blue_xy;
    uint8_t red_y;
    uint8_t red_x;
    uint8_t green_y;
    uint8_t green_x;
    uint8_t blue_y;
    uint8_t blue_x;
    uint8_t white_y;
    uint8_t white_x;
} __packed;

struct edid_detailed_timings {
    uint8_t hori_freq;
    uint8_t vert_freq;
    uint8_t hactive;
    uint8_t hblank;
    uint8_t hactive_hblank;
    uint8_t vactive;
    uint8_t vblank;
    uint8_t vactive_vblank;
    uint8_t hsync_offset;
    uint8_t hsync_width;
    uint8_t vsyncoffs_vsyncw;
    uint8_t vhsyncoffs_width;
    uint8_t himage_size_mm;
    uint8_t vimage_size_mm;
    uint8_t himage_vimage_size;
    uint8_t hborder;
    uint8_t vborder;
    uint8_t display_type;
} __packed;

struct edid {
    uint8_t padding[8];
    uint16_t manufacturer_id;
    uint16_t edid_id;
    uint32_t serial_number;
    uint8_t manufactured_week;
    uint8_t manufactured_year;
    uint8_t edid_version;
    uint8_t edid_revision;
    uint8_t video_input_type;
    uint8_t max_phys_width;
    uint8_t max_phys_height;
    uint8_t gamma_factor;
    uint8_t dpms_flags;
    struct edid_chroma_info chroma_info;
    uint8_t established_timings[2];
    uint8_t reserved_timing;
    uint16_t standard_timings[8];
    struct edid_detailed_timings detailed_timings[4];
    uint8_t unused;
    uint8_t checksum;
} __packed;

#define DDC_CAP_DDC1       0x01
#define DDC_CAP_DDC2       0x02
#define DDC_CAP_XFER_BLANK 0x04

status_t VlBiosP_GetVbeControllerInfo(struct vbe_controller_info *buf);

status_t VlBiosP_GetVbeVideoModeInfo(uint16_t mode, struct vbe_video_mode_info *buf);

status_t VlBiosP_SetVbeVideoMode(uint16_t mode);

status_t VlBiosP_GetVbeVideoMode(uint16_t *mode);

status_t VlBiosP_SetVbeDisplayStart(uint16_t x, uint16_t y);

status_t VlBiosP_SetVbeDisplayStartAtVsync(uint16_t x, uint16_t y);

status_t VlBiosP_ScheduleVbeDisplayStart(uint32_t fboffset);

status_t VlBiosP_ScheduleVbeDisplayStartAtVsync(uint32_t fboffset);

status_t VlBiosP_GetVbePmiTable(struct VlA_FarPtr16 *pmi_table, uint16_t *size);

status_t VlBiosP_CheckVbeDdcCapability(uint16_t ctrlr_unit, uint8_t *xfer_time, uint8_t *flags);

status_t VlBiosP_GetVbeDdcEdid(uint16_t ctrlr_unit, uint16_t edid_block, struct edid *buf);

#endif  // __VELLUM_ASM_BIOS_VIDEO_H__
