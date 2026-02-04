#ifndef __STRATA_MM_H__
#define __STRATA_MM_H__

#include <stddef.h>
#include <stdint.h>

#include <strata/arch/mmu.h>
#include <strata/plat/mm.h>

#include <strata/compiler.h>
#include <strata/status.h>
#include <strata/types.h>

#include <strata/mm/asp.h>
#include <strata/mm/pmm.h>
#include <strata/mm/pool.h>
#include <strata/mm/types.h>
#include <strata/mm/utils.h>
#include <strata/mm/vmm.h>

StStatus StMm_Init(void);

StStatus StMm_GlobalVirtPageToPhysFrame(St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional);
StStatus StMm_LocalVirtPageToPhysFrame(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional
);

__always_inline StStatus
StMm_GlobalVirtAddrToPhysAddr(uintptr_t vaddr __in, uintptr_t *paddr __out_optional)
{
    StStatus status;
    St_PhysFrame pfn;

    status = StMm_GlobalVirtPageToPhysFrame(ADDR_TO_PAGE(vaddr), &pfn);
    if (!CHECK_SUCCESS(status)) return status;

    *paddr = PAGE_TO_ADDR(pfn) + (vaddr % PAGE_SIZE);

    return STATUS_SUCCESS;
}
__always_inline StStatus StMm_LocalVirtAddrToPhysAddr(
    struct StMm_AddressSpace *asp __in, uintptr_t vaddr __in, uintptr_t *paddr __out_optional
)
{
    StStatus status;
    St_PhysFrame pfn;

    status = StMm_LocalVirtPageToPhysFrame(asp, ADDR_TO_PAGE(vaddr), &pfn);
    if (!CHECK_SUCCESS(status)) return status;

    *paddr = PAGE_TO_ADDR(pfn) + (vaddr % PAGE_SIZE);

    return STATUS_SUCCESS;
}

StStatus StMm_MapGlobal(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StVmm_AllocFlags vmmflags __in,
    StMm_MapFlags mapflags __in
);
StStatus StMm_MapLocal(
    struct StMm_AddressSpace *asp __in,
    St_VirtPage *vpn __out,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StVmm_AllocFlags vmmflags __in,
    StMm_MapFlags mapflags __in
);
StStatus StMm_MapGlobalTo(
    St_VirtPage vpn __in,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StVmm_AllocFlags vmmflags __in,
    StMm_MapFlags mapflags __in
);
StStatus StMm_MapLocalTo(
    struct StMm_AddressSpace *asp __in,
    St_VirtPage vpn __in,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StVmm_AllocFlags vmmflags __in,
    StMm_MapFlags mapflags __in
);
void StMm_UnmapGlobal(St_VirtPage vpn __in, St_PageCount count __in);
void StMm_UnmapLocal(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PageCount count __in
);

StStatus StMm_AllocateGlobalContiguous(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StPmm_AllocFlags pmmflags __in,
    StVmm_AllocFlags vmmflags __in,
    StMm_MapFlags mapflags __in
);
StStatus StMm_AllocateLocalContiguous(
    struct StMm_AddressSpace *asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StPmm_AllocFlags pmmflags __in,
    StVmm_AllocFlags vmmflags __in,
    StMm_MapFlags mapflags __in
);
StStatus StMm_AllocateGlobalSparse(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StPmm_AllocFlags pmmflags __in,
    StVmm_AllocFlags vmmflags __in,
    StMm_MapFlags mapflags __in
);
StStatus StMm_AllocateLocalSparse(
    struct StMm_AddressSpace *asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StPmm_AllocFlags pmmflags __in,
    StVmm_AllocFlags vmmflags __in,
    StMm_MapFlags mapflags __in
);
StStatus StMm_AllocateGlobalContiguousTo(
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StPmm_AllocFlags pmmflags __in,
    StMm_MapFlags mapflags __in
);
StStatus StMm_AllocateLocalContiguousTo(
    struct StMm_AddressSpace *asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StPmm_AllocFlags pmmflags __in,
    StMm_MapFlags mapflags __in
);
StStatus StMm_AllocateGlobalSparseTo(
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StPmm_AllocFlags pmmflags __in,
    StMm_MapFlags mapflags __in
);
StStatus StMm_AllocateLocalSparseTo(
    struct StMm_AddressSpace *asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StPmm_AllocFlags pmmflags __in,
    StMm_MapFlags mapflags __in
);

void StMm_FreeGlobal(St_VirtPage vpn __in, St_PageCount count __in);
void StMm_FreeLocal(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PageCount count __in
);

StStatus StMm_SetGlobalPageFlags(
    St_VirtPage vpn __in, St_PageCount count __in, StMm_MapFlags mapflags __in
);
StStatus StMm_SetLocalPageFlags(
    struct StMm_AddressSpace *asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
);

StStatus StMm_GetGlobalPageFlags(St_VirtPage vpn __in, StMm_MapFlags *mapflags __out);
StStatus StMm_GetLocalPageFlags(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, StMm_MapFlags *mapflags __out
);

#endif  // __STRATA_MM_H__
