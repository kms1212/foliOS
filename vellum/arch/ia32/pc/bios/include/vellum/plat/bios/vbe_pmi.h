#ifndef __VELLUM_ASM_BIOS_VBE_PMI_H__
#define __VELLUM_ASM_BIOS_VBE_PMI_H__

#include <stdint.h>

#include <vellum/arch/farptr.h>

#include <vellum/plat/bios/video.h>

int VlBiosP_SetVbePmiMemoryWindow(
    struct VlA_FarPtr16 pmi_table, int window, uint16_t memory_window
);

int VlBiosP_SetVbePmiDisplayStart(struct VlA_FarPtr16 pmi_table, uint32_t offset);

int VlBiosP_SetVbePmiDisplayStartAtVsync(struct VlA_FarPtr16 pmi_table, uint32_t offset);

int VlBiosP_SetVbePmiPaletteData(
    struct VlA_FarPtr16 pmi_table,
    int palette,
    uint16_t start,
    uint16_t count,
    const struct vbe_palette_entry *data
);

int VlBiosP_SetVbePmiPaletteDataAtVsync(
    struct VlA_FarPtr16 pmi_table,
    int palette,
    uint16_t start,
    uint16_t count,
    const struct vbe_palette_entry *data
);

#endif  // __VELLUM_ASM_BIOS_VBE_PMI_H__
