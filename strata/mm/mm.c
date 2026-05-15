#include <strata/mm.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <strata/arch/mmu_constants.h>

#include <strata/plat/memmap.h>
#include <strata/plat/mm.h>

#include <strata/compiler.h>
#include <strata/log.h>
#include <strata/mm/address_space_refs.h>
#include <strata/mm/address_space.h>
#include <strata/mm/allocation_owner.h>
#include <strata/mm/allocation_owner_refs.h>
#include <strata/mm/pmm.h>
#include <strata/mm/pmm_refs.h>
#include <strata/mm/pool.h>
#include <strata/mm/types.h>
#include <strata/mm/vmm.h>
#include <strata/panic.h>
#include <strata/process.h>
#include <strata/ref_control.h>
#include <strata/status.h>

#include "internal.h"

#define MODULE_NAME "mm"

#define PAGE_FAULT_ERROR_PRESENT ((uint64_t)1 << 0)
#define PHYS_TO_DIRECTMAP_PTR(pa)                                                                  \
    ((void *)((uintptr_t)(pa) + PAGE_TO_ADDR(MEMMAP_DIRECTMAP_VPN_BASE)))

static void finalize_allocation_owner(void *object __in)
{
    StPool_Free(object);
}

static St_PageCount get_sparse_alloc_batch_count(St_PageCount remaining_count)
{
    if (remaining_count == 0) return 0;

    return (St_PageCount)1ULL
        << ((sizeof(unsigned long long) * 8) - 1 - __builtin_clzll(remaining_count));
}

static void *phys_frame_to_directmap_ptr(St_PhysFrame pfn __in)
{
    return PHYS_TO_DIRECTMAP_PTR(FRAME_TO_ADDR(pfn));
}

static StStatus validate_image_backing(
    St_VirtPage vpn __in,
    St_PageCount count __in,
    const struct StMm_ImageBacking *backing __in
)
{
    uintptr_t range_start;
    uintptr_t range_end;
    uintptr_t content_end;
    size_t range_size;

    assert(backing);

    if (count == 0) return STATUS_INVALID_VALUE;
    if (count > (St_PageCount)(SIZE_MAX / PAGE_SIZE)) return STATUS_TOO_LARGE;

    range_start = PAGE_TO_ADDR(vpn);
    range_size = (size_t)count * PAGE_SIZE;
    if (range_size > UINTPTR_MAX - range_start) return STATUS_INVALID_VALUE;
    range_end = range_start + range_size;

    if (backing->content_size && !backing->base) return STATUS_INVALID_VALUE;
    if (backing->content_offset > backing->size) return STATUS_INVALID_VALUE;
    if (backing->content_size > backing->size - backing->content_offset) {
        return STATUS_INVALID_VALUE;
    }
    if (backing->content_size > UINTPTR_MAX - backing->content_addr) {
        return STATUS_INVALID_VALUE;
    }

    content_end = backing->content_addr + backing->content_size;
    if (backing->content_addr < range_start) return STATUS_INVALID_VALUE;
    if (content_end > range_end) return STATUS_INVALID_VALUE;

    return STATUS_SUCCESS;
}

static StStatus fill_image_backed_frame(
    St_PhysFrame pfn __in,
    St_VirtPage vpn __in,
    const struct StMm_ImageBacking *backing __in
)
{
    uintptr_t page_start;
    uintptr_t page_end;
    uintptr_t content_start;
    uintptr_t content_end;
    uintptr_t overlap_start;
    uintptr_t overlap_end;
    size_t copy_len;
    size_t source_offset;
    uint8_t *frame;

    assert(backing);

    frame = phys_frame_to_directmap_ptr(pfn);
    memset(frame, 0, PAGE_SIZE);

    if (!backing->content_size) return STATUS_SUCCESS;
    if (!backing->base) return STATUS_INVALID_VALUE;
    if (backing->content_offset > backing->size) return STATUS_INVALID_VALUE;
    if (backing->content_size > backing->size - backing->content_offset) {
        return STATUS_INVALID_VALUE;
    }
    if (backing->content_size > UINTPTR_MAX - backing->content_addr) {
        return STATUS_INVALID_VALUE;
    }

    page_start = PAGE_TO_ADDR(vpn);
    page_end = page_start + PAGE_SIZE;
    content_start = backing->content_addr;
    content_end = backing->content_addr + backing->content_size;

    if (page_end <= content_start || content_end <= page_start) return STATUS_SUCCESS;

    overlap_start = page_start > content_start ? page_start : content_start;
    overlap_end = page_end < content_end ? page_end : content_end;
    copy_len = (size_t)(overlap_end - overlap_start);
    source_offset = backing->content_offset + (size_t)(overlap_start - content_start);

    if (source_offset > backing->size) return STATUS_INVALID_VALUE;
    if (copy_len > backing->size - source_offset) return STATUS_INVALID_VALUE;

    memcpy(
        frame + (size_t)(overlap_start - page_start),
        (const uint8_t *)backing->base + source_offset,
        copy_len
    );

    return STATUS_SUCCESS;
}

static StStatus allocate_sparse_frame_batch(
    St_PhysFrame *pfn __out,
    St_PageCount *allocated_count __out,
    St_PageCount remaining_count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in
)
{
    assert(pfn);
    assert(allocated_count);

    StStatus status;
    St_PageCount batch_count = get_sparse_alloc_batch_count(remaining_count);

    while (batch_count > 0) {
        status =
            StPmm_AllocateContiguousFrame(pfn, batch_count, owner, alloc_flags & ~AF_ALIGN_MASK);
        if (CHECK_SUCCESS(status)) {
            *allocated_count = batch_count;
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
    StPmm_AllocationMetadata_BorrowedRef metadata;
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
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount allocated_count __in
)
{
    StStatus status;
    St_PhysFrame pfn;
    StPmm_AllocationMetadata_BorrowedRef metadata;
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
    return StAddressSpace_InitBase();
}

StStatus StMm_GlobalVirtPageToPhysFrame(St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional)
{
    return StMmP_GlobalVirtPageToPhysFrame(vpn, pfn);
}

StStatus StMm_LocalVirtPageToPhysFrame(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PhysFrame *pfn __out_optional
)
{
    return StMmP_LocalVirtPageToPhysFrame(asp, vpn, pfn);
}

StStatus StMm_MapGlobal(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    struct StMm_CompoundFlags flags __in
)
{
    assert(vpn);

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
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    assert(vpn);

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
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    assert(vpn);

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
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    assert(vpn);

    StStatus status;
    St_PhysFrame allocated_pfn = (St_PhysFrame)-1;
    St_VirtPage allocated_vpn = (St_VirtPage)-1;
    size_t allocated_count = 0;
    St_PageCount batch_count = 0;
    StAllocationOwner_StrongRef owner;

    if (!asp) return STATUS_INVALID_VALUE;

    if (!(map_flags & MF_IMMEDIATE)) {
        if (!(map_flags & MF_ZERO_FILL)) return STATUS_NOT_SUPPORTED;

        status = StVmm_AllocateLocalPage(asp, &allocated_vpn, count, alloc_flags, map_flags);
        if (!CHECK_SUCCESS(status)) return status;

        status = StMmP_MapLocalSparseMemory(asp, allocated_vpn, count, map_flags);
        if (!CHECK_SUCCESS(status)) {
            StMmP_UnmapLocalSparseMemory(asp, allocated_vpn, count);
            StVmm_FreeLocalPage(asp, allocated_vpn, count);
            return status;
        }

        *vpn = allocated_vpn;

        LOG_TRACE(
            LM_CAT_UNCLASSIFIED,
            "allocated %zu lazy pages to %013zX\n",
            count,
            (uintptr_t)allocated_vpn
        );

        return STATUS_SUCCESS;
    }

    owner = asp->process->alloc_owner;

    status = StVmm_AllocateLocalPage(asp, &allocated_vpn, count, alloc_flags, map_flags);
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
    StAllocationOwner_StrongRef owner __in,
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
    StAddressSpace_StrongRef asp __in,
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
    StAllocationOwner_StrongRef owner;
    int vpn_allocated = 0;

    if (!asp) return STATUS_INVALID_VALUE;
    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    if (!(map_flags & MF_IMMEDIATE)) {
        if (!(map_flags & MF_ZERO_FILL)) return STATUS_NOT_SUPPORTED;

        status = StVmm_AllocateLocalPageTo(asp, vpn, count, alloc_flags, map_flags);
        if (!CHECK_SUCCESS(status)) return status;

        status = StMmP_MapLocalSparseMemory(asp, vpn, count, map_flags);
        if (!CHECK_SUCCESS(status)) {
            StMmP_UnmapLocalSparseMemory(asp, vpn, count);
            StVmm_FreeLocalPage(asp, vpn, count);
            return status;
        }

        LOG_TRACE(
            LM_CAT_UNCLASSIFIED,
            "allocated %zu lazy pages to %013zX\n",
            count,
            (uintptr_t)vpn
        );

        return STATUS_SUCCESS;
    }

    owner = asp->process->alloc_owner;

    status = StVmm_AllocateLocalPageTo(asp, vpn, count, alloc_flags, map_flags);
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

StStatus StMm_AllocateLocalImageTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    const struct StMm_ImageBacking *backing __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    assert(backing);

    StStatus status;
    StMm_MapFlags effective_map_flags = map_flags | MF_ZERO_FILL;

    if (!asp) return STATUS_INVALID_VALUE;
    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;
    if (map_flags & MF_IMMEDIATE) return STATUS_INVALID_VALUE;

    status = validate_image_backing(vpn, count, backing);
    if (!CHECK_SUCCESS(status)) return status;

    status = StVmm_AllocateLocalPageTo(asp, vpn, count, alloc_flags, effective_map_flags);
    if (!CHECK_SUCCESS(status)) return status;

    status = StVmm_SetLocalPageImageBacking(asp, vpn, count, backing);
    if (!CHECK_SUCCESS(status)) {
        StVmm_FreeLocalPage(asp, vpn, count);
        return status;
    }

    status = StMmP_MapLocalSparseMemory(asp, vpn, count, effective_map_flags);
    if (!CHECK_SUCCESS(status)) {
        StMmP_UnmapLocalSparseMemory(asp, vpn, count);
        StVmm_FreeLocalPage(asp, vpn, count);
        return status;
    }

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "allocated %zu image pages to %013zX\n", count, (uintptr_t)vpn);

    return STATUS_SUCCESS;
}

StStatus StMm_HandlePageFault(
    StAddressSpace_StrongRef asp __in, uintptr_t fault_addr __in, uint64_t error_code __in
)
{
    StStatus status;
    St_VirtPage vpn = ADDR_TO_PAGE(fault_addr);
    struct StVmm_PageInfo page_info;
    St_PhysFrame pfn = (St_PhysFrame)-1;
    StAllocationOwner_StrongRef owner;

    if (!asp) return STATUS_INVALID_VALUE;
    if (!IS_LOCAL_VPN(vpn)) return STATUS_PAGE_NOT_PRESENT;
    if (error_code & PAGE_FAULT_ERROR_PRESENT) return STATUS_NOT_PERMITTED;

    status = StVmm_GetLocalPageInfo(asp, vpn, &page_info);
    if (!CHECK_SUCCESS(status)) return status;
    if (page_info.backing_type != VMM_BACKING_DEMAND_ZERO &&
        page_info.backing_type != VMM_BACKING_IMAGE) {
        return STATUS_PAGE_NOT_PRESENT;
    }

    status = StMm_LocalVirtPageToPhysFrame(asp, vpn, NULL);
    if (CHECK_SUCCESS(status)) return STATUS_SUCCESS;
    if (status != STATUS_PAGE_NOT_PRESENT) return status;

    owner = asp->process->alloc_owner;
    status = StPmm_AllocateContiguousFrame(
        &pfn,
        (St_PageCount)1,
        owner,
        page_info.alloc_flags & ~AF_VMM_HIDDEN_AT_MAP
    );
    if (!CHECK_SUCCESS(status)) return status;

    if (page_info.backing_type == VMM_BACKING_IMAGE) {
        status = fill_image_backed_frame(pfn, vpn, &page_info.image_backing);
        if (!CHECK_SUCCESS(status)) {
            StPmm_FreeContiguousFrame(pfn);
            return status;
        }
    }

    status = StMmP_MapLocalContiguousMemory(
        asp,
        pfn,
        vpn,
        (St_PageCount)1,
        page_info.backing_type == VMM_BACKING_IMAGE ? page_info.map_flags & ~MF_ZERO_FILL
                                                    : page_info.map_flags | MF_ZERO_FILL
    );
    if (!CHECK_SUCCESS(status)) {
        StPmm_FreeContiguousFrame(pfn);
        return status;
    }

    return STATUS_SUCCESS;
}

void StMm_FreeGlobal(enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in)
{
    StStatus status;
    St_PhysFrame pfn;
    StPmm_AllocationMetadata_BorrowedRef metadata;
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
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
)
{
    StStatus status;
    St_PhysFrame pfn;
    StPmm_AllocationMetadata_BorrowedRef metadata;
    struct StVmm_PageInfo page_info;
    size_t page_count_to_free;
    size_t i = 0;
    int allow_unmapped_pages = 0;

    if (!IS_LOCAL_VPN(vpn)) return;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "freeing %zu pages at %013zX\n", count, (uintptr_t)vpn);

    status = StVmm_GetLocalPageInfo(asp, vpn, &page_info);
    if (CHECK_SUCCESS(status) &&
        (page_info.backing_type == VMM_BACKING_DEMAND_ZERO ||
         page_info.backing_type == VMM_BACKING_IMAGE)) {
        allow_unmapped_pages = 1;
    }

    while (i < count) {
        status = StMm_LocalVirtPageToPhysFrame(asp, vpn + i, &pfn);
        if (status == STATUS_PAGE_NOT_PRESENT && allow_unmapped_pages) {
            i++;
            continue;
        }
        if (!CHECK_SUCCESS(status)) {
            St_Panic(STATUS_CONFLICTING_STATE, "tried to free unmapped page");
        }

        status = StPmm_GetAllocMetadata(pfn, &metadata);
        if (!CHECK_SUCCESS(status)) {
            St_Panic(STATUS_CONFLICTING_STATE, "pmm allocation metadata unavailable");
        }

        page_count_to_free = (size_t)1 << metadata->order;
        if (page_count_to_free > count - i) page_count_to_free = count - i;
        StPmm_FreeContiguousFrame(pfn);
        StMmP_UnmapLocalContiguousMemory(asp, vpn + i, (St_PageCount)page_count_to_free);

        i += page_count_to_free;
    }

    if (allow_unmapped_pages) {
        StMmP_UnmapLocalSparseMemory(asp, vpn, count);
    }

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
    StAddressSpace_StrongRef asp __in,
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
    assert(map_flags);

    if (!IS_GLOBAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return StMmP_GetGlobalPageFlags(vpn, map_flags);
}

StStatus StMm_GetLocalPageFlags(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, StMm_MapFlags *map_flags __out
)
{
    assert(map_flags);

    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    return StMmP_GetLocalPageFlags(asp, vpn, map_flags);
}

StStatus StAllocationOwner_Create(StAllocationOwner_StrongRef *owner __out)
{
    assert(owner);

    StStatus status;
    StAllocationOwner_StrongRef new_owner;

    status = StPool_AllocateClear(sizeof(*new_owner), (void **)&new_owner);
    if (!CHECK_SUCCESS(status)) return status;

    StRefControlBlock_Init(&new_owner->ref_control, 1, new_owner, finalize_allocation_owner);

    *owner = new_owner;

    return STATUS_SUCCESS;
}

void StAllocationOwner_Acquire(StAllocationOwner_StrongRef owner __inout)
{
    assert(owner);

    StRefControlBlock_Acquire(&owner->ref_control);
}

void StAllocationOwner_Release(StAllocationOwner_StrongRef owner __inout)
{
    assert(owner);

    (void)StRefControlBlock_Release(&owner->ref_control);
}

int StAllocationOwner_IsClosed(StAllocationOwner_StrongRef owner __in)
{
    assert(owner);

    return StRefControlBlock_IsDying(&owner->ref_control);
}

void StAllocationOwner_Close(StAllocationOwner_StrongRef owner __in)
{
    struct vmm_alloc_node *node, *next;

    if (!owner) return;

    StRefControlBlock_MarkDying(&owner->ref_control);

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "cleaning up owner allocations\n");

    node = owner->first_vmm_node;
    while (node) {
        next = node->owner_next;

        if (node->alloc_type == AT_ALLOC) {
            if (node->asp) {
                StMm_FreeLocal(
                    (StAddressSpace_StrongRef)node->asp,
                    node->base_vpn,
                    (St_PageCount)(node->limit_vpn - node->base_vpn)
                );
            } else {
                StMm_FreeGlobal(
                    node->domain,
                    node->base_vpn,
                    (St_PageCount)(node->limit_vpn - node->base_vpn)
                );
            }
        } else {
            if (node->asp) {
                // StMm_UnmapLocal(
                //     (StAddressSpace_StrongRef)node->asp,
                //     node->base_vpn,
                //     node->limit_vpn - node->base_vpn
                // );
            } else {
                StMm_UnmapGlobal(
                    node->domain,
                    node->base_vpn,
                    (St_PageCount)(node->limit_vpn - node->base_vpn)
                );
            }
        }
        node = next;
    }
}
