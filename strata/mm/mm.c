#include <strata/mm.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <strata/arch/mmu.h>

#include <strata/plat/memmap.h>
#include <strata/plat/mm.h>

#include <strata/log.h>
#include <strata/macros.h>
#include <strata/panic.h>

#include "internal.h"

#define MODULE_NAME "mm"

StStatus StMm_Init(void)
{
    return StMm_InitBaseAddressSpace();
}

StStatus StMm_GlobalVirtPageToPhysFrame(St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional)
{
    return StMmP_GlobalVirtPageToPhysFrame(vpn, pfn);
}

StStatus StMm_LocalVirtPageToPhysFrame(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional
)
{
    return StMmP_LocalVirtPageToPhysFrame(asp, vpn, pfn);
}

StStatus StMm_MapGlobal(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    struct StMm_AllocationOwner *owner __in,
    struct StMm_CompoundFlags flags __in
)
{
    StStatus status;
    St_VirtPage allocated_vpn = (St_VirtPage)-1;

    status = StVmm_AllocateGlobalPage(
        domain,
        &allocated_vpn,
        count,
        owner,
        flags.alloc_flags | AF_VMM_HIDDEN_AT_MAP
    );
    if (!CHECK_SUCCESS(status)) return status;

    *vpn = allocated_vpn;

    status = StMmP_MapGlobalContiguousMemory(pfn, allocated_vpn, count, flags.map_flags);
    if (!CHECK_SUCCESS(status)) return status;

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "allocated %zu pages to %013zX\n",
        count,
        (uintptr_t)allocated_vpn
    );

    return STATUS_SUCCESS;
}

void StMm_UnmapGlobal(enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in)
{
    LOG_TRACE(LM_CAT_UNCLASSIFIED, "unmapping page %013zX (count=%zu)\n", (uintptr_t)vpn, count);

    StMmP_UnmapGlobalContiguousMemory(vpn, count);

    StVmm_FreeGlobalPage(domain, vpn, count);
}

StStatus StMm_AllocateGlobalContiguous(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    struct StMm_AllocationOwner *owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    StStatus status;
    St_VirtPage allocated_vpn = (St_VirtPage)-1;
    St_PhysFrame allocated_pfn = (St_PhysFrame)-1;

    status = StPmm_AllocateContiguousFrame(&allocated_pfn, count, owner, alloc_flags);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StMm_MapGlobal(
        domain,
        &allocated_vpn,
        allocated_pfn,
        count,
        owner,
        (struct StMm_CompoundFlags){alloc_flags, map_flags}
    );
    if (!CHECK_SUCCESS(status)) goto has_error;

    *vpn = allocated_vpn;

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "allocated %zu pages to %013zX\n",
        count,
        (uintptr_t)allocated_vpn
    );

    return STATUS_SUCCESS;

has_error:
    if (allocated_vpn != (St_VirtPage)-1) {
        StMm_UnmapGlobal(domain, allocated_vpn, count);
    }

    if (allocated_pfn != (St_PhysFrame)-1) {
        StPmm_FreeContiguousFrame(allocated_pfn);
    }

    return status;
}

// TODO: apply adaptive batch decay
StStatus StMm_AllocateGlobalSparse(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    struct StMm_AllocationOwner *owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    StStatus status;
    St_PhysFrame allocated_pfn = (St_PhysFrame)-1;
    St_VirtPage allocated_vpn = (St_VirtPage)-1;
    size_t allocated_count = 0;

    /* allocate virtual memory pages first */
    status = StVmm_AllocateGlobalPage(domain, &allocated_vpn, count, owner, alloc_flags);
    if (!CHECK_SUCCESS(status)) goto has_error;

    /* it will allow allocating non-contiguous physical memory frames */
    /* if you want to allocate/map frames/pages for hardware I/O, */
    /* you should use StPmm_AllocateContiguous() */
    for (; allocated_count < count; allocated_count++) {
        status = StMm_GlobalVirtPageToPhysFrame(allocated_vpn + (St_VirtPage)allocated_count, NULL);
        if (status != STATUS_PAGE_NOT_PRESENT) goto has_error;

        status = StPmm_AllocateContiguousFrame(
            &allocated_pfn,
            (St_PageCount)1,
            owner,
            alloc_flags & ~AF_ALIGN_MASK
        );
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = StMmP_MapGlobalContiguousMemory(
            allocated_pfn,
            allocated_vpn + (St_VirtPage)allocated_count,
            1,
            map_flags
        );
        if (!CHECK_SUCCESS(status)) {
            StPmm_FreeContiguousFrame(allocated_pfn);
            goto has_error;
        }
    }

    *vpn = allocated_vpn;

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "allocated %zu pages to %013zX\n",
        count,
        (uintptr_t)allocated_vpn
    );

    return STATUS_SUCCESS;

has_error:
    /* allocation failed. rollback changes */
    for (size_t i = 0; i < allocated_count; i++) {
        /* read vpn-pfn mapping to know which frames to free */
        if (!CHECK_SUCCESS(StMm_GlobalVirtPageToPhysFrame(allocated_vpn + i, &allocated_pfn)))
            continue;

        /* unmap vpn & free frame*/
        StPmm_FreeContiguousFrame(allocated_pfn);
    }

    StMmP_UnmapGlobalContiguousMemory(allocated_vpn, allocated_count);

    if (allocated_vpn != (St_VirtPage)-1) {
        StVmm_FreeGlobalPage(domain, allocated_vpn, count);
    }

    return status;
}

// TODO: apply adaptive batch decay
StStatus StMm_AllocateGlobalSparseTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    struct StMm_AllocationOwner *owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    StStatus status;
    St_PhysFrame allocated_pfn = (St_PhysFrame)-1;
    size_t allocated_count = 0;
    int vpn_allocated = 0;

    if (!IS_GLOBAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    status = StVmm_AllocateGlobalPageTo(domain, vpn, count, owner, alloc_flags);
    if (!CHECK_SUCCESS(status)) goto has_error;
    vpn_allocated = 1;

    for (; allocated_count < count; allocated_count++) {
        status = StMm_GlobalVirtPageToPhysFrame(vpn + (St_VirtPage)allocated_count, NULL);
        if (status != STATUS_PAGE_NOT_PRESENT) goto has_error;

        status = StPmm_AllocateContiguousFrame(
            &allocated_pfn,
            (St_PageCount)1,
            owner,
            alloc_flags & ~AF_ALIGN_MASK
        );
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = StMmP_MapGlobalContiguousMemory(
            allocated_pfn,
            vpn + (St_VirtPage)allocated_count,
            1,
            map_flags
        );
        if (!CHECK_SUCCESS(status)) {
            StPmm_FreeContiguousFrame(allocated_pfn);
            goto has_error;
        }
    }

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "allocated %zu pages to %013zX\n", count, (uintptr_t)vpn);

    return STATUS_SUCCESS;

has_error:
    /* allocation failed. rollback changes */
    for (size_t i = 0; i < allocated_count; i++) {
        /* read vpn-pfn mapping to know which frames to free */
        if (!CHECK_SUCCESS(StMm_GlobalVirtPageToPhysFrame(vpn + i, &allocated_pfn))) continue;

        /* unmap vpn & free frame*/
        StPmm_FreeContiguousFrame(allocated_pfn);
    }

    StMmP_UnmapGlobalContiguousMemory(vpn, allocated_count);

    if (vpn_allocated) {
        StVmm_FreeGlobalPage(domain, vpn, count);
    }

    return status;
}

// TODO: apply adaptive batch decay
StStatus StMm_AllocateLocalSparseTo(
    struct StMm_AddressSpace *asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    StStatus status;
    St_PhysFrame allocated_pfn = (St_PhysFrame)-1;
    size_t allocated_count = 0;
    struct StMm_AllocationOwner *owner = &asp->process->alloc_owner;
    int vpn_allocated = 0;

    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    status = StVmm_AllocateLocalPageTo(asp, vpn, count, alloc_flags);
    if (!CHECK_SUCCESS(status)) goto has_error;
    vpn_allocated = 1;

    for (; allocated_count < count; allocated_count++) {
        status = StMm_LocalVirtPageToPhysFrame(asp, vpn + (St_VirtPage)allocated_count, NULL);
        if (status != STATUS_PAGE_NOT_PRESENT) goto has_error;

        status = StPmm_AllocateContiguousFrame(
            &allocated_pfn,
            (St_PageCount)1,
            owner,
            alloc_flags & ~AF_ALIGN_MASK
        );
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = StMmP_MapLocalContiguousMemory(
            asp,
            allocated_pfn,
            vpn + (St_VirtPage)allocated_count,
            1,
            map_flags
        );
        if (!CHECK_SUCCESS(status)) {
            StPmm_FreeContiguousFrame(allocated_pfn);
            goto has_error;
        }
    }

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "allocated %zu pages to %013zX\n", count, (uintptr_t)vpn);

    return STATUS_SUCCESS;

has_error:
    /* allocation failed. rollback changes */
    for (size_t i = 0; i < allocated_count; i++) {
        /* read vpn-pfn mapping to know which frames to free */
        if (!CHECK_SUCCESS(StMm_LocalVirtPageToPhysFrame(asp, vpn + i, &allocated_pfn))) continue;

        /* unmap vpn & free frame*/
        StPmm_FreeContiguousFrame(allocated_pfn);
    }

    StMmP_UnmapLocalContiguousMemory(asp, vpn, allocated_count);

    if (vpn_allocated) {
        StVmm_FreeLocalPage(asp, vpn, count);
    }

    return status;
}

void StMm_FreeGlobal(enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in)
{
    StStatus status;
    St_PhysFrame pfn;
    struct StPmm_AllocationMetadata *metadata;
    size_t i = 0;

    if (!IS_GLOBAL_VPN(vpn)) return;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "freeing %zu pages at %013zX\n", count, (uintptr_t)vpn);

    while (i < count) {
        status = StMm_GlobalVirtPageToPhysFrame(vpn + i, &pfn);
        if (!CHECK_SUCCESS(status)) {
            St_Panic(STATUS_CONFLICTING_STATE, "tried to free unmapped page");
        }

        status = StPmm_GetAllocMetadata(pfn, &metadata);
        if (!CHECK_SUCCESS(status)) {
            St_Panic(STATUS_CONFLICTING_STATE, "pmm allocation metadata unavailable");
        }

        StPmm_FreeContiguousFrame(pfn);

        i += 1 << metadata->order;
    }

    StMmP_UnmapGlobalContiguousMemory(vpn, count);

    StVmm_FreeGlobalPage(domain, vpn, count);
}

void StMm_FreeLocal(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PageCount count __in
)
{
    StStatus status;
    St_PhysFrame pfn;
    struct StPmm_AllocationMetadata *metadata;
    size_t i = 0;

    if (!IS_LOCAL_VPN(vpn)) return;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "freeing %zu pages at %013zX\n", count, (uintptr_t)vpn);

    while (i < count) {
        status = StMm_LocalVirtPageToPhysFrame(asp, vpn + i, &pfn);
        if (!CHECK_SUCCESS(status)) {
            St_Panic(STATUS_CONFLICTING_STATE, "tried to free unmapped page");
        }

        status = StPmm_GetAllocMetadata(pfn, &metadata);
        if (!CHECK_SUCCESS(status)) {
            St_Panic(STATUS_CONFLICTING_STATE, "pmm allocation metadata unavailable");
        }

        StPmm_FreeContiguousFrame(pfn);

        i += 1 << metadata->order;
    }

    StMmP_UnmapLocalContiguousMemory(asp, vpn, count);

    StVmm_FreeLocalPage(asp, vpn, count);
}

StStatus StMm_SetGlobalPageFlags(
    St_VirtPage vpn __in, St_PageCount count __in, StMm_MapFlags mapflags __in
)
{
    StStatus status;

    if (!IS_GLOBAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    status = StMmP_RemapGlobalContiguousMemory(vpn, count, mapflags);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

StStatus StMm_SetLocalPageFlags(
    struct StMm_AddressSpace *asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_MapFlags mapflags __in
)
{
    StStatus status;

    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    status = StMmP_RemapLocalContiguousMemory(asp, vpn, count, mapflags);
    if (!CHECK_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

void StMm_CleanupOwnerAllocation(struct StMm_AllocationOwner *owner __in)
{
    struct vmm_alloc_node *node, *next;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "cleaning up owner allocations\n");

    node = owner->first_vmm_node;
    while (node) {
        next = node->owner_next;

        if (node->alloc_type == AT_ALLOC) {
            if (node->asp) {
                StMm_FreeLocal(node->asp, node->base_vpn, node->limit_vpn - node->base_vpn);
            } else {
                StMm_FreeGlobal(node->domain, node->base_vpn, node->limit_vpn - node->base_vpn);
            }
        } else {
            if (node->asp) {
                // StMm_UnmapLocal(node->asp, node->base_vpn, node->limit_vpn - node->base_vpn);
            } else {
                StMm_UnmapGlobal(node->domain, node->base_vpn, node->limit_vpn - node->base_vpn);
            }
        }
        node = next;
    }
}
