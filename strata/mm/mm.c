#include <strata/mm.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <strata/plat/memmap.h>
#include <strata/plat/mm.h>

#include <strata/compiler.h>
#include <strata/log.h>
#include <strata/mm/asp.h>
#include <strata/mm/owner.h>
#include <strata/mm/pmm.h>
#include <strata/mm/types.h>
#include <strata/mm/vmm.h>
#include <strata/panic.h>
#include <strata/status.h>

#include "internal.h"

#define MODULE_NAME "mm"

static St_PageCount get_sparse_alloc_batch_count(St_PageCount remaining_count)
{
    if (remaining_count == 0) return 0;

    return (St_PageCount)1ULL
        << ((sizeof(unsigned long long) * 8) - 1 - __builtin_clzll(remaining_count));
}

static StStatus allocate_sparse_frame_batch(
    St_PhysFrame *pfn __out,
    St_PageCount *allocated_count __out,
    St_PageCount remaining_count __in,
    struct StMm_AllocationOwner *owner __in,
    StMm_AllocFlags alloc_flags __in
)
{
    StStatus status;
    St_PageCount batch_count = get_sparse_alloc_batch_count(remaining_count);

    while (batch_count > 0) {
        status =
            StPmm_AllocateContiguousFrame(pfn, batch_count, owner, alloc_flags & ~AF_ALIGN_MASK);
        if (CHECK_SUCCESS(status)) {
            if (allocated_count) *allocated_count = batch_count;
            return STATUS_SUCCESS;
        }

        if (status != STATUS_INSUFFICIENT_MEMORY) return status;

        batch_count >>= 1;
    }

    return STATUS_INSUFFICIENT_MEMORY;
}

static void rollback_global_sparse_allocation(
    St_VirtPage vpn __in, St_PageCount allocated_count __in
)
{
    StStatus status;
    St_PhysFrame pfn;
    struct StPmm_AllocationMetadata *metadata;
    size_t page_count_to_free;
    size_t i = 0;

    while (i < allocated_count) {
        status = StMm_GlobalVirtPageToPhysFrame(vpn + i, &pfn);
        if (!CHECK_SUCCESS(status)) {
            i++;
            continue;
        }

        status = StPmm_GetAllocMetadata(pfn, &metadata);
        if (!CHECK_SUCCESS(status)) {
            StPmm_FreeContiguousFrame(pfn);
            i++;
            continue;
        }

        page_count_to_free = (size_t)1 << metadata->order;
        StPmm_FreeContiguousFrame(pfn);
        i += page_count_to_free;
    }
}

static void rollback_local_sparse_allocation(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, St_PageCount allocated_count __in
)
{
    StStatus status;
    St_PhysFrame pfn;
    struct StPmm_AllocationMetadata *metadata;
    size_t page_count_to_free;
    size_t i = 0;

    while (i < allocated_count) {
        status = StMm_LocalVirtPageToPhysFrame(asp, vpn + i, &pfn);
        if (!CHECK_SUCCESS(status)) {
            i++;
            continue;
        }

        status = StPmm_GetAllocMetadata(pfn, &metadata);
        if (!CHECK_SUCCESS(status)) {
            StPmm_FreeContiguousFrame(pfn);
            i++;
            continue;
        }

        page_count_to_free = (size_t)1 << metadata->order;
        StPmm_FreeContiguousFrame(pfn);
        i += page_count_to_free;
    }
}

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
    St_PageCount batch_count = 0;

    /* allocate virtual memory pages first */
    status = StVmm_AllocateGlobalPage(domain, &allocated_vpn, count, owner, alloc_flags);
    if (!CHECK_SUCCESS(status)) {
        LOG_ERROR(
            LM_CAT_UNCLASSIFIED,
            "StMm_AllocateGlobalSparse: StVmm_AllocateGlobalPage failed (domain=%d count=%zu "
            "status=%08X)\n",
            domain,
            count,
            status
        );
        goto has_error;
    }

    /* it will allow allocating non-contiguous physical memory frames */
    /* if you want to allocate/map frames/pages for hardware I/O, */
    /* you should use StPmm_AllocateContiguous() */
    while (allocated_count < count) {
        batch_count = get_sparse_alloc_batch_count(count - allocated_count);

        for (size_t i = 0; i < batch_count; i++) {
            status = StMm_GlobalVirtPageToPhysFrame(
                allocated_vpn + (St_VirtPage)allocated_count + (St_VirtPage)i,
                NULL
            );
            if (status != STATUS_PAGE_NOT_PRESENT) {
                LOG_ERROR(
                    LM_CAT_UNCLASSIFIED,
                    "StMm_AllocateGlobalSparse: expected page-not-present at vpn=%013zX but got "
                    "%08X\n",
                    (uintptr_t)(allocated_vpn + (St_VirtPage)allocated_count + (St_VirtPage)i),
                    status
                );
                goto has_error;
            }
        }

        status = allocate_sparse_frame_batch(
            &allocated_pfn,
            &batch_count,
            count - allocated_count,
            owner,
            alloc_flags
        );
        if (!CHECK_SUCCESS(status)) {
            LOG_ERROR(
                LM_CAT_UNCLASSIFIED,
                "StMm_AllocateGlobalSparse: allocate_sparse_frame_batch failed (status=%08X, "
                "i=%zu)\n",
                status,
                allocated_count
            );
            goto has_error;
        }

        status = StMmP_MapGlobalContiguousMemory(
            allocated_pfn,
            allocated_vpn + (St_VirtPage)allocated_count,
            batch_count,
            map_flags
        );
        if (!CHECK_SUCCESS(status)) {
            LOG_ERROR(
                LM_CAT_UNCLASSIFIED,
                "StMm_AllocateGlobalSparse: StMmP_MapGlobalContiguousMemory failed (status=%08X, "
                "vpn=%013zX, pfn=%013zX, count=%zu)\n",
                status,
                (uintptr_t)(allocated_vpn + (St_VirtPage)allocated_count),
                (uintptr_t)allocated_pfn,
                batch_count
            );
            StPmm_FreeContiguousFrame(allocated_pfn);
            goto has_error;
        }

        allocated_count += batch_count;
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
    rollback_global_sparse_allocation(allocated_vpn, allocated_count);

    StMmP_UnmapGlobalContiguousMemory(allocated_vpn, allocated_count);

    if (allocated_vpn != (St_VirtPage)-1) {
        StVmm_FreeGlobalPage(domain, allocated_vpn, count);
    }

    return status;
}

StStatus StMm_AllocateLocalSparse(
    struct StMm_AddressSpace *asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    StStatus status;
    St_PhysFrame allocated_pfn = (St_PhysFrame)-1;
    St_VirtPage allocated_vpn = (St_VirtPage)-1;
    size_t allocated_count = 0;
    St_PageCount batch_count = 0;
    struct StMm_AllocationOwner *owner;

    if (!asp || !vpn) return STATUS_INVALID_VALUE;

    owner = &asp->process->alloc_owner;

    status = StVmm_AllocateLocalPage(asp, &allocated_vpn, count, alloc_flags);
    if (!CHECK_SUCCESS(status)) goto has_error;

    while (allocated_count < count) {
        batch_count = get_sparse_alloc_batch_count(count - allocated_count);

        for (size_t i = 0; i < batch_count; i++) {
            status = StMm_LocalVirtPageToPhysFrame(
                asp,
                allocated_vpn + (St_VirtPage)allocated_count + (St_VirtPage)i,
                NULL
            );
            if (status != STATUS_PAGE_NOT_PRESENT) goto has_error;
        }

        status = allocate_sparse_frame_batch(
            &allocated_pfn,
            &batch_count,
            count - allocated_count,
            owner,
            alloc_flags
        );
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = StMmP_MapLocalContiguousMemory(
            asp,
            allocated_pfn,
            allocated_vpn + (St_VirtPage)allocated_count,
            batch_count,
            map_flags
        );
        if (!CHECK_SUCCESS(status)) {
            StPmm_FreeContiguousFrame(allocated_pfn);
            goto has_error;
        }

        allocated_count += batch_count;
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
    rollback_local_sparse_allocation(asp, allocated_vpn, allocated_count);

    if (allocated_vpn != (St_VirtPage)-1) {
        StMmP_UnmapLocalContiguousMemory(asp, allocated_vpn, allocated_count);
        StVmm_FreeLocalPage(asp, allocated_vpn, count);
    }

    return status;
}

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
    St_PageCount batch_count = 0;
    int vpn_allocated = 0;

    if (!IS_GLOBAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    status = StVmm_AllocateGlobalPageTo(domain, vpn, count, owner, alloc_flags);
    if (!CHECK_SUCCESS(status)) goto has_error;
    vpn_allocated = 1;

    while (allocated_count < count) {
        batch_count = get_sparse_alloc_batch_count(count - allocated_count);

        for (size_t i = 0; i < batch_count; i++) {
            status = StMm_GlobalVirtPageToPhysFrame(
                vpn + (St_VirtPage)allocated_count + (St_VirtPage)i,
                NULL
            );
            if (status != STATUS_PAGE_NOT_PRESENT) goto has_error;
        }

        status = allocate_sparse_frame_batch(
            &allocated_pfn,
            &batch_count,
            count - allocated_count,
            owner,
            alloc_flags
        );
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = StMmP_MapGlobalContiguousMemory(
            allocated_pfn,
            vpn + (St_VirtPage)allocated_count,
            batch_count,
            map_flags
        );
        if (!CHECK_SUCCESS(status)) {
            StPmm_FreeContiguousFrame(allocated_pfn);
            goto has_error;
        }

        allocated_count += batch_count;
    }

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "allocated %zu pages to %013zX\n", count, (uintptr_t)vpn);

    return STATUS_SUCCESS;

has_error:
    /* allocation failed. rollback changes */
    rollback_global_sparse_allocation(vpn, allocated_count);

    StMmP_UnmapGlobalContiguousMemory(vpn, allocated_count);

    if (vpn_allocated) {
        StVmm_FreeGlobalPage(domain, vpn, count);
    }

    return status;
}

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
    St_PageCount batch_count = 0;
    struct StMm_AllocationOwner *owner = &asp->process->alloc_owner;
    int vpn_allocated = 0;

    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    status = StVmm_AllocateLocalPageTo(asp, vpn, count, alloc_flags);
    if (!CHECK_SUCCESS(status)) goto has_error;
    vpn_allocated = 1;

    while (allocated_count < count) {
        batch_count = get_sparse_alloc_batch_count(count - allocated_count);

        for (size_t i = 0; i < batch_count; i++) {
            status = StMm_LocalVirtPageToPhysFrame(
                asp,
                vpn + (St_VirtPage)allocated_count + (St_VirtPage)i,
                NULL
            );
            if (status != STATUS_PAGE_NOT_PRESENT) goto has_error;
        }

        status = allocate_sparse_frame_batch(
            &allocated_pfn,
            &batch_count,
            count - allocated_count,
            owner,
            alloc_flags
        );
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = StMmP_MapLocalContiguousMemory(
            asp,
            allocated_pfn,
            vpn + (St_VirtPage)allocated_count,
            batch_count,
            map_flags
        );
        if (!CHECK_SUCCESS(status)) {
            StPmm_FreeContiguousFrame(allocated_pfn);
            goto has_error;
        }

        allocated_count += batch_count;
    }

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "allocated %zu pages to %013zX\n", count, (uintptr_t)vpn);

    return STATUS_SUCCESS;

has_error:
    /* allocation failed. rollback changes */
    rollback_local_sparse_allocation(asp, vpn, allocated_count);

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
    size_t page_count_to_free;
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

        page_count_to_free = (size_t)1 << metadata->order;
        StPmm_FreeContiguousFrame(pfn);

        i += page_count_to_free;
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
    size_t page_count_to_free;
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

        page_count_to_free = (size_t)1 << metadata->order;
        StPmm_FreeContiguousFrame(pfn);

        i += page_count_to_free;
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

StStatus StMm_GetGlobalPageFlags(St_VirtPage vpn __in, StMm_MapFlags *map_flags __out)
{
    if (!IS_GLOBAL_VPN(vpn) || !map_flags) return STATUS_INVALID_VALUE;

    return StMmP_GetGlobalPageFlags(vpn, map_flags);
}

StStatus StMm_GetLocalPageFlags(
    struct StMm_AddressSpace *asp __in, St_VirtPage vpn __in, StMm_MapFlags *map_flags __out
)
{
    if (!IS_LOCAL_VPN(vpn) || !map_flags) return STATUS_INVALID_VALUE;

    return StMmP_GetLocalPageFlags(asp, vpn, map_flags);
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
