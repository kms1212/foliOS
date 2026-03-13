#ifndef __VELLUM_ASM_BIOS_VBE_PMI_H__
#define __VELLUM_ASM_BIOS_VBE_PMI_H__

#include <stdint.h>

#include <vellum/arch/farptr.h>

#include <vellum/plat/bios/video.h>

int _pc_vbe_pmi_set_memory_window(
    struct VlA_FarPtr16 pmi_table, int window, uint16_t memory_window
);

int _pc_vbe_pmi_set_display_start(struct VlA_FarPtr16 pmi_table, uint32_t offset);

int _pc_vbe_pmi_set_display_start_vsync(struct VlA_FarPtr16 pmi_table, uint32_t offset);

int _pc_vbe_pmi_set_palette_data(
    struct VlA_FarPtr16 pmi_table,
    int palette,
    uint16_t start,
    uint16_t count,
    const struct vbe_palette_entry *data
);

int _pc_vbe_pmi_set_palette_data_vsync(
    struct VlA_FarPtr16 pmi_table,
    int palette,
    uint16_t start,
    uint16_t count,
    const struct vbe_palette_entry *data
);

#endif  // __VELLUM_ASM_BIOS_VBE_PMI_H__
