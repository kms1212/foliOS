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

/** Initialize Strata memory management after boot handoff data is available. */
StStatus StMm_Init(void);

/** Translate a global virtual page to a physical frame if it is present. */
StStatus StMm_GlobalVirtPageToPhysFrame(St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional);
/** Translate a local address-space virtual page to a physical frame if present. */
StStatus StMm_LocalVirtPageToPhysFrame(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional
);

/** Translate a global virtual byte address to a physical byte address. */
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
/** Translate a local virtual byte address to a physical byte address. */
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

/**
 * Map caller-owned physical frames into a VMM domain.
 *
 * Map calls reserve virtual space and install mappings for pfn/count. They do
 * not allocate PMM backing and must be paired with Unmap.
 */
StStatus StMm_MapGlobal(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    struct StMm_CompoundFlags flags __in
);
/** Map caller-owned physical frames into a local address space. */
StStatus StMm_MapLocal(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Map caller-owned physical frames at a chosen global virtual page. */
StStatus StMm_MapGlobalTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Map caller-owned physical frames at a chosen local virtual page. */
StStatus StMm_MapLocalTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Release a global mapping created by StMm_MapGlobal*. */
void StMm_UnmapGlobal(enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in);
/** Release a local mapping created by StMm_MapLocal*. */
void StMm_UnmapLocal(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
);

/**
 * Allocate virtual space and physical backing together.
 *
 * Allocate calls return usable memory and must be paired with Free. Local sparse
 * and image-backed allocations may materialize physical frames lazily unless
 * MF_IMMEDIATE is set.
 */
StStatus StMm_AllocateGlobalContiguous(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Allocate contiguous local physical backing and map it into an address space. */
StStatus StMm_AllocateLocalContiguous(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Allocate sparse global physical backing and map it into a VMM domain. */
StStatus StMm_AllocateGlobalSparse(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Allocate sparse local memory, possibly using demand-zero policy. */
StStatus StMm_AllocateLocalSparse(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Allocate contiguous global memory at a chosen virtual page. */
StStatus StMm_AllocateGlobalContiguousTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Allocate contiguous local memory at a chosen virtual page. */
StStatus StMm_AllocateLocalContiguousTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Allocate sparse global memory at a chosen virtual page. */
StStatus StMm_AllocateGlobalSparseTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Allocate sparse local memory at a chosen virtual page. */
StStatus StMm_AllocateLocalSparseTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Reserve and lazily materialize a local image-backed range. */
StStatus StMm_AllocateLocalImageTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    const struct StMm_ImageBacking *image_backing __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);

/**
 * Reserve virtual address space and mapping policy without committing backing.
 *
 * Reserve calls must be paired with Commit+Free or Free. Reserve by itself does
 * not mean demand paging; the VMM policy and mapping flags decide how later
 * faults or commits are handled.
 */
StStatus StMm_ReserveGlobalContiguous(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Reserve a contiguous local virtual range. */
StStatus StMm_ReserveLocalContiguous(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Reserve a sparse global virtual range. */
StStatus StMm_ReserveGlobalSparse(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Reserve a sparse local virtual range. */
StStatus StMm_ReserveLocalSparse(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Reserve a contiguous global range at a chosen virtual page. */
StStatus StMm_ReserveGlobalContiguousTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Reserve a contiguous local range at a chosen virtual page. */
StStatus StMm_ReserveLocalContiguousTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Reserve a sparse global range at a chosen virtual page. */
StStatus StMm_ReserveGlobalSparseTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Reserve a sparse local range at a chosen virtual page. */
StStatus StMm_ReserveLocalSparseTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
);
/** Commit physical backing to a previously reserved global range. */
StStatus StMm_CommitGlobal(
    enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in
);
/** Commit physical backing to a previously reserved local range. */
StStatus StMm_CommitLocal(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
);

/** Free global memory acquired through Allocate or Reserve. */
void StMm_FreeGlobal(enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in);
/** Free local memory acquired through Allocate or Reserve. */
void StMm_FreeLocal(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
);

/** Update mapping flags for present global pages. */
StStatus StMm_SetGlobalPageFlags(
    St_VirtPage vpn __in, St_PageCount count __in, StMm_MapFlags mapflags __in
);
/** Update mapping flags for present local pages. */
StStatus StMm_SetLocalPageFlags(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags map_flags __in
);

/** Read mapping flags for a present global page. */
StStatus StMm_GetGlobalPageFlags(St_VirtPage vpn __in, StMm_MapFlags *map_flags __out);
/** Read mapping flags for a present local page. */
StStatus StMm_GetLocalPageFlags(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, StMm_MapFlags *map_flags __out
);

/** Handle a local not-present page fault if MM/VMM policy can resolve it. */
StStatus StMm_HandlePageFault(
    StAddressSpace_StrongRef asp __in, uintptr_t fault_addr __in, uint64_t error_code __in
);

#endif  // __STRATA_MM_H__
