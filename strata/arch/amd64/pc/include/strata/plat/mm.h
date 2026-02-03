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

StStatus StMmP_CreateAddressSpace(struct StMm_AddressSpace *asp __in);
void StMmP_RemoveAddressSpace(struct StMm_AddressSpace *asp __in);
StStatus StMmP_SwitchAddressSpace(struct StMm_AddressSpace *asp __in);

StStatus StMmP_GlobalVirtPageToPhysFrame(St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional);
StStatus StMmP_LocalVirtPageToPhysFrame(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional
);

StStatus StMmP_MapGlobalMemory(
    St_PhysFrame pfn __in, St_VirtPage vpn __in, StMm_MapFlags mapflags __in
);
StStatus StMmP_MapLocalMemory(
    struct StMm_AddressSpace *asp __in,
    St_PhysFrame pfn __in,
    St_VirtPage vpn __in,
    StMm_MapFlags mapflags __in
);

StStatus StMmP_RemapGlobalMemory(St_VirtPage vpn __in, StMm_MapFlags mapflags __in);
StStatus StMmP_RemapLocalMemory(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, StMm_MapFlags mapflags __in
);

void StMmP_UnmapGlobalMemory(St_VirtPage vpn __in);
void StMmP_UnmapLocalMemory(struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in);

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
