#ifndef __STRATA_PLAT_MM_H__
#define __STRATA_PLAT_MM_H__

#include <strata/plat/cpulocal.h>

#include <strata/status.h>
#include <strata/types.h>

#include <strata/mm/types.h>

struct StMmP_AddressSpace {
    St_PhysFrame root_table_pfn;
};

#ifndef __STRATA_MM_ADDRESS_SPACE_REFS_DEFINED__
#    define __STRATA_MM_ADDRESS_SPACE_REFS_DEFINED__
struct StMm_AddressSpace;
typedef struct StMm_AddressSpace *StMm_AddressSpace_StrongRef __ref_strong;
typedef struct StMm_AddressSpace *StMm_AddressSpace_WeakRef __ref_weak;
typedef struct StMm_AddressSpace *StMm_AddressSpace_BorrowedRef __ref_borrowed;
typedef struct StMm_AddressSpace *StMm_AddressSpace_InternalRef __ref_internal;
#endif

StStatus StMmP_CleanupTempMapping(void);

StStatus StMmP_InitBaseAddressSpace(void);
St_PageCount StMmP_ReclaimCachedPageTableFrames(St_PageCount page_budget __in);

StStatus StMmP_CreateAddressSpace(StMm_AddressSpace_StrongRef asp __in);
void StMmP_RemoveAddressSpace(StMm_AddressSpace_StrongRef asp __in);
StStatus StMmP_SwitchAddressSpace(StMm_AddressSpace_StrongRef asp __in);

StStatus StMmP_GlobalVirtPageToPhysFrame(St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional);
StStatus StMmP_LocalVirtPageToPhysFrame(
    StMm_AddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional
);
StStatus StMmP_GetGlobalPageFlags(St_VirtPage vpn __in, StMm_MapFlags *map_flags __out);
StStatus StMmP_GetLocalPageFlags(
    StMm_AddressSpace_StrongRef asp __in, St_VirtPage vpn __in, StMm_MapFlags *map_flags __out
);

StStatus StMmP_MapGlobalContiguousMemory(
    St_PhysFrame pfn __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
);
StStatus StMmP_MapLocalContiguousMemory(
    StMm_AddressSpace_StrongRef asp __in,
    St_PhysFrame pfn __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
);

StStatus StMmP_RemapGlobalContiguousMemory(
    St_VirtPage vpn __in, St_PageCount count __in, StMm_MapFlags mapflags __in
);
StStatus StMmP_RemapLocalContiguousMemory(
    StMm_AddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
);

void StMmP_UnmapGlobalContiguousMemory(St_VirtPage vpn __in, St_PageCount count __in);
void StMmP_UnmapLocalContiguousMemory(
    StMm_AddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
);

StStatus StMmP_ReadLocal(
    StMm_AddressSpace_StrongRef asp __in, uintptr_t addr __in, void *buf __buf, size_t len __in
);

StStatus StMmP_WriteLocal(
    StMm_AddressSpace_StrongRef asp __in, uintptr_t addr __in, const void *buf __in, size_t len __in
);

StStatus StMmP_SetLocal(
    StMm_AddressSpace_StrongRef asp __in, uintptr_t addr __in, int value __in, size_t len __in
);

StStatus StMmP_CopyLocal(
    StMm_AddressSpace_StrongRef dest_asp __in,
    uintptr_t dest __in,
    StMm_AddressSpace_StrongRef src_asp __in,
    uintptr_t src __in,
    size_t len __in
);

StStatus StMmP_MapConventionalMemory(St_VirtPage *vpn __out);

#endif  // __STRATA_PLAT_MM_H__
