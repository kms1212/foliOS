#ifndef __STRATA_PLAT_MMU_H__
#define __STRATA_PLAT_MMU_H__

#include <strata/mm.h>
#include <strata/status.h>

StStatus StMmP_InitBaseAddressSpace(void);
StStatus StMmP_VirtPageToPhysFrame(St_VirtPage vpn, St_PhysFrame *pfn);
StStatus StMmP_MapMemory(St_PhysFrame pfn, St_VirtPage vpn, uint32_t flags);
StStatus StMmP_UnmapMemory(St_PhysFrame pfn, St_VirtPage vpn, uint32_t flags);

#endif  // __STRATA_PLAT_MMU_H__
