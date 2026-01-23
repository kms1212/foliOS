#ifndef __VELLUM_ASM_BIOS_VBE_PMI_H__
#define __VELLUM_ASM_BIOS_VBE_PMI_H__

#include <stdint.h>

#include <vellum/asm/farptr.h>
#include <vellum/asm/bios/video.h>

int _pc_vbe_pmi_set_memory_window(farptr16_t pmi_table, int window, uint16_t memory_window);

int _pc_vbe_pmi_set_display_start(farptr16_t pmi_table, uint32_t offset);

int _pc_vbe_pmi_set_display_start_vsync(farptr16_t pmi_table, uint32_t offset);

int _pc_vbe_pmi_set_palette_data(farptr16_t pmi_table, int palette, uint16_t start, uint16_t count, const struct vbe_palette_entry *data);

int _pc_vbe_pmi_set_palette_data_vsync(farptr16_t pmi_table, int palette, uint16_t start, uint16_t count, const struct vbe_palette_entry *data);

#endif // __VELLUM_ASM_BIOS_VBE_PMI_H__
