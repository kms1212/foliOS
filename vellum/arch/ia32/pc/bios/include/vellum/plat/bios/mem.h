#ifndef __VELLUM_ASM_BIOS_MEM_H__
#define __VELLUM_ASM_BIOS_MEM_H__

#include <stdint.h>

#include <vellum/status.h>

struct smap_entry {
    uint32_t base_addr_low;
    uint32_t base_addr_high;
    uint32_t length_low;
    uint32_t length_high;
    uint32_t type;
};

status_t VlBiosP_QueryMemoryMap(uint32_t *cursor, struct smap_entry *buf, long buf_size);

#endif  // __VELLUM_ASM_BIOS_MEM_H__
