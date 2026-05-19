#ifndef __STRATA_PLAT_MM_H__
#define __STRATA_PLAT_MM_H__

#include <strata/plat/cpulocal.h>

#include <strata/status.h>
#include <strata/types.h>

#include <strata/mm/address_space_refs.h>
#include <strata/mm/types.h>

struct StAddressSpaceP_PlatformData {
    St_PhysFrame root_table_pfn;
};

StStatus StMmP_CleanupTempMapping(void);

StStatus StAddressSpaceP_InitBase(void);
St_PageCount StMmP_ReclaimCachedPageTableFrames(St_PageCount page_budget __in);

StStatus StAddressSpaceP_Create(StAddressSpace_StrongRef asp __in);
void StAddressSpaceP_Remove(StAddressSpace_StrongRef asp __in);
StStatus StAddressSpaceP_Switch(StAddressSpace_StrongRef asp __in);

StStatus StMmP_GlobalVirtPageToPhysFrame(St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional);
StStatus StMmP_LocalVirtPageToPhysFrame(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional
);
StStatus StMmP_GetGlobalPageMapFlags(St_VirtPage vpn __in, StMm_MapFlags *map_flags __out);
StStatus StMmP_GetLocalPageMapFlags(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, StMm_MapFlags *map_flags __out
);

StStatus StMmP_MapGlobalContiguousMemory(
    St_PhysFrame pfn __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
);
StStatus StMmP_MapLocalContiguousMemory(
    StAddressSpace_StrongRef asp __in,
    St_PhysFrame pfn __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
);
StStatus StMmP_MapGlobalSparseMemory(
    St_VirtPage vpn __in, St_PageCount count __in, StMm_MapFlags mapflags __in
);
StStatus StMmP_MapLocalSparseMemory(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
);

StStatus StMmP_RemapGlobalContiguousMemory(
    St_VirtPage vpn __in, St_PageCount count __in, StMm_MapFlags mapflags __in
);
StStatus StMmP_RemapLocalContiguousMemory(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
);

void StMmP_UnmapGlobalContiguousMemory(St_VirtPage vpn __in, St_PageCount count __in);
void StMmP_UnmapLocalContiguousMemory(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
);
void StMmP_UnmapGlobalSparseMemory(St_VirtPage vpn __in, St_PageCount count __in);
void StMmP_UnmapLocalSparseMemory(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
);

StStatus StMmP_ReadLocal(
    StAddressSpace_StrongRef asp __in, uintptr_t addr __in, void *buf __buf, size_t len __in
);

StStatus StMmP_WriteLocal(
    StAddressSpace_StrongRef asp __in, uintptr_t addr __in, const void *buf __in, size_t len __in
);

StStatus StMmP_SetLocal(
    StAddressSpace_StrongRef asp __in, uintptr_t addr __in, int value __in, size_t len __in
);

StStatus StMmP_CopyLocal(
    StAddressSpace_StrongRef dest_asp __in,
    uintptr_t dest __in,
    StAddressSpace_StrongRef src_asp __in,
    uintptr_t src __in,
    size_t len __in
);

#endif  // __STRATA_PLAT_MM_H__
