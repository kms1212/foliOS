#ifndef __STRATA_PLAT_MM_H__
#define __STRATA_PLAT_MM_H__

#include <strata/plat/cpulocal.h>

#include <strata/status.h>
#include <strata/types.h>

#include <strata/mm/types.h>

struct StMmP_AddressSpace {
    St_PhysFrame root_table_pfn;
};

struct StMm_AddressSpace;

StStatus StMmP_CleanupTempMapping(void);

StStatus StMmP_InitBaseAddressSpace(void);
St_PageCount StMmP_ReclaimCachedPageTableFrames(St_PageCount page_budget __in);

StStatus StMmP_CreateAddressSpace(struct StMm_AddressSpace *asp __in);
void StMmP_RemoveAddressSpace(struct StMm_AddressSpace *asp __in);
StStatus StMmP_SwitchAddressSpace(struct StMm_AddressSpace *asp __in);

StStatus StMmP_GlobalVirtPageToPhysFrame(St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional);
StStatus StMmP_LocalVirtPageToPhysFrame(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional
);
StStatus StMmP_GetGlobalPageFlags(St_VirtPage vpn __in, StMm_MapFlags *map_flags __out);
StStatus StMmP_GetLocalPageFlags(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, StMm_MapFlags *map_flags __out
);

StStatus StMmP_MapGlobalContiguousMemory(
    St_PhysFrame pfn __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
);
StStatus StMmP_MapLocalContiguousMemory(
    struct StMm_AddressSpace *asp __in,
    St_PhysFrame pfn __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
);

StStatus StMmP_RemapGlobalContiguousMemory(
    St_VirtPage vpn __in, St_PageCount count __in, StMm_MapFlags mapflags __in
);
StStatus StMmP_RemapLocalContiguousMemory(
    struct StMm_AddressSpace *asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
);

void StMmP_UnmapGlobalContiguousMemory(St_VirtPage vpn __in, St_PageCount count __in);
void StMmP_UnmapLocalContiguousMemory(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PageCount count __in
);

StStatus StMmP_ReadLocal(
    struct StMm_AddressSpace *asp __in, uintptr_t addr __in, void *buf __buf, size_t len __in
);

StStatus StMmP_WriteLocal(
    struct StMm_AddressSpace *asp __in, uintptr_t addr __in, const void *buf __in, size_t len __in
);

StStatus StMmP_SetLocal(
    struct StMm_AddressSpace *asp __in, uintptr_t addr __in, int value, size_t len __in
);

StStatus StMmP_CopyLocal(
    struct StMm_AddressSpace *dest_asp __in,
    uintptr_t dest __in,
    struct StMm_AddressSpace *src_asp __in,
    uintptr_t src __in,
    size_t len __in
);

#endif  // __STRATA_PLAT_MM_H__
