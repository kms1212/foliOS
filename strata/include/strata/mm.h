#ifndef __STRATA_MM_H__
#define __STRATA_MM_H__

#include <stddef.h>
#include <stdint.h>

#include <strata/arch/mmu.h>
#include <strata/plat/mm.h>

#include <strata/compiler.h>
#include <strata/status.h>
#include <strata/types.h>

#include <strata/mm/address_space.h>
#include <strata/mm/allocation_owner.h>
#include <strata/mm/pmm.h>
#include <strata/mm/pool.h>
#include <strata/mm/types.h>
#include <strata/mm/utils.h>
#include <strata/mm/vmm.h>

StStatus StMm_Init(void);

StStatus StMm_GlobalVirtPageToPhysFrame(St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional);
StStatus StMm_LocalVirtPageToPhysFrame(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional
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
    StAddressSpace_StrongRef asp __in, uintptr_t vaddr __in, uintptr_t *paddr __out_optional
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
    StAllocationOwner_StrongRef owner __in,
    struct StMm_CompoundFlags flags __in
);
StStatus StMm_MapLocal(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_MapGlobalTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_MapLocalTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
void StMm_UnmapGlobal(enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in);
void StMm_UnmapLocal(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
);

StStatus StMm_AllocateGlobalContiguous(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_AllocateLocalContiguous(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_AllocateGlobalSparse(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_AllocateLocalSparse(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_AllocateGlobalContiguousTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_AllocateLocalContiguousTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_AllocateGlobalSparseTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_AllocateLocalSparseTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_AllocateLocalImageTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    const struct StMm_ImageBacking *image_backing __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);

StStatus StMm_ReserveGlobalContiguous(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_ReserveLocalContiguous(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_ReserveGlobalSparse(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_ReserveLocalSparse(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_ReserveGlobalContiguousTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_ReserveLocalContiguousTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_ReserveGlobalSparseTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_ReserveLocalSparseTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
StStatus StMm_CommitGlobal(
    enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in
);
StStatus StMm_CommitLocal(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
);

void StMm_FreeGlobal(enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in);
void StMm_FreeLocal(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
);

StStatus StMm_SetGlobalPageFlags(
    St_VirtPage vpn __in, St_PageCount count __in, StMm_MapFlags mapflags __in
);
StStatus StMm_SetLocalPageFlags(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags map_flags __in
);

StStatus StMm_GetGlobalPageFlags(St_VirtPage vpn __in, StMm_MapFlags *map_flags __out);
StStatus StMm_GetLocalPageFlags(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, StMm_MapFlags *map_flags __out
);

StStatus StMm_HandlePageFault(
    StAddressSpace_StrongRef asp __in, uintptr_t fault_addr __in, uint64_t error_code __in
);

#endif  // __STRATA_MM_H__
