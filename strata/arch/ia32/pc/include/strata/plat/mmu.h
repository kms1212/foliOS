#ifndef __STRATA_PLAT_MMU_H__
#define __STRATA_PLAT_MMU_H__

#include <strata/mm.h>
#include <strata/status.h>

StStatus StMmuP_Init(void);
StStatus StMmuP_VirtPageToPhysFrame(St_VirtPage vpn, St_PhysFrame *pfn);
StStatus StMmuP_MapMemory(St_PhysFrame pfn, St_VirtPage vpn, uint32_t flags);
StStatus StMmuP_UnmapMemory(St_PhysFrame pfn, St_VirtPage vpn, uint32_t flags);

#endif  // __STRATA_PLAT_MMU_H__
