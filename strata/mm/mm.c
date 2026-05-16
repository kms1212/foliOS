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
#include <strata/mm/address_space.h>
#include <strata/mm/address_space_refs.h>
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
    const struct StMm_ImageBacking *image_backing __in
)
{
    uintptr_t range_start;
    uintptr_t range_end;
    uintptr_t content_end;
    size_t range_size;

    assert(image_backing);

    if (count == 0) return STATUS_INVALID_VALUE;
    if (count > (St_PageCount)(SIZE_MAX / PAGE_SIZE)) return STATUS_TOO_LARGE;

    range_start = PAGE_TO_ADDR(vpn);
    range_size = (size_t)count * PAGE_SIZE;
    if (range_size > UINTPTR_MAX - range_start) return STATUS_INVALID_VALUE;
    range_end = range_start + range_size;

    if (image_backing->content_size && !image_backing->base) return STATUS_INVALID_VALUE;
    if (image_backing->content_offset > image_backing->size) return STATUS_INVALID_VALUE;
    if (image_backing->content_size > image_backing->size - image_backing->content_offset) {
        return STATUS_INVALID_VALUE;
    }
    if (image_backing->content_size > UINTPTR_MAX - image_backing->content_addr) {
        return STATUS_INVALID_VALUE;
    }

    content_end = image_backing->content_addr + image_backing->content_size;
    if (image_backing->content_addr < range_start) return STATUS_INVALID_VALUE;
    if (content_end > range_end) return STATUS_INVALID_VALUE;

    return STATUS_SUCCESS;
}

static StStatus fill_image_backed_frame(
    St_PhysFrame pfn __in, St_VirtPage vpn __in, const struct StMm_ImageBacking *image_backing __in
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

    assert(image_backing);

    frame = phys_frame_to_directmap_ptr(pfn);
    memset(frame, 0, PAGE_SIZE);

    if (!image_backing->content_size) return STATUS_SUCCESS;
    if (!image_backing->base) return STATUS_INVALID_VALUE;
    if (image_backing->content_offset > image_backing->size) return STATUS_INVALID_VALUE;
    if (image_backing->content_size > image_backing->size - image_backing->content_offset) {
        return STATUS_INVALID_VALUE;
    }
    if (image_backing->content_size > UINTPTR_MAX - image_backing->content_addr) {
        return STATUS_INVALID_VALUE;
    }

    page_start = PAGE_TO_ADDR(vpn);
    page_end = page_start + PAGE_SIZE;
    content_start = image_backing->content_addr;
    content_end = image_backing->content_addr + image_backing->content_size;

    if (page_end <= content_start || content_end <= page_start) return STATUS_SUCCESS;

    overlap_start = page_start > content_start ? page_start : content_start;
    overlap_end = page_end < content_end ? page_end : content_end;
    copy_len = (size_t)(overlap_end - overlap_start);
    source_offset = image_backing->content_offset + (size_t)(overlap_start - content_start);

    if (source_offset > image_backing->size) return STATUS_INVALID_VALUE;
    if (copy_len > image_backing->size - source_offset) return STATUS_INVALID_VALUE;

    memcpy(
        frame + (size_t)(overlap_start - page_start),
        (const uint8_t *)image_backing->base + source_offset,
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

    status = StVmm_ReserveGlobalPage(
        domain,
        &allocated_vpn,
        count,
        owner,
        flags.alloc_flags | AF_VMM_RESERVATION_MAP,
        flags.map_flags
    );
    if (!CHECK_SUCCESS(status)) return status;

    status = StMmP_MapGlobalContiguousMemory(pfn, allocated_vpn, count, flags.map_flags);
    if (!CHECK_SUCCESS(status)) {
        StVmm_ReleaseGlobalPage(domain, allocated_vpn, count);
        return status;
    }

    *vpn = allocated_vpn;

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "allocated %zu pages to %013zX\n",
        count,
        (uintptr_t)allocated_vpn
    );

    return STATUS_SUCCESS;
}

StStatus StMm_MapLocal(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    assert(vpn);

    StStatus status;
    St_VirtPage reserved_vpn = (St_VirtPage)-1;

    if (!asp) return STATUS_INVALID_VALUE;

    status = StVmm_ReserveLocalPage(
        asp,
        &reserved_vpn,
        count,
        alloc_flags | AF_VMM_RESERVATION_MAP,
        map_flags
    );
    if (!CHECK_SUCCESS(status)) return status;

    status = StMmP_MapLocalContiguousMemory(asp, pfn, reserved_vpn, count, map_flags);
    if (!CHECK_SUCCESS(status)) {
        StVmm_ReleaseLocalPage(asp, reserved_vpn, count);
        return status;
    }

    *vpn = reserved_vpn;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "mapped %zu pages to %013zX\n", count, (uintptr_t)reserved_vpn);

    return STATUS_SUCCESS;
}

StStatus StMm_MapGlobalTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    StStatus status;

    status = StVmm_ReserveGlobalPageTo(
        domain,
        vpn,
        count,
        owner,
        alloc_flags | AF_VMM_RESERVATION_MAP,
        map_flags
    );
    if (!CHECK_SUCCESS(status)) return status;

    status = StMmP_MapGlobalContiguousMemory(pfn, vpn, count, map_flags);
    if (!CHECK_SUCCESS(status)) {
        StVmm_ReleaseGlobalPage(domain, vpn, count);
        return status;
    }

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "mapped %zu pages to %013zX\n", count, (uintptr_t)vpn);

    return STATUS_SUCCESS;
}

StStatus StMm_MapLocalTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    StStatus status;

    if (!asp) return STATUS_INVALID_VALUE;

    status =
        StVmm_ReserveLocalPageTo(asp, vpn, count, alloc_flags | AF_VMM_RESERVATION_MAP, map_flags);
    if (!CHECK_SUCCESS(status)) return status;

    status = StMmP_MapLocalContiguousMemory(asp, pfn, vpn, count, map_flags);
    if (!CHECK_SUCCESS(status)) {
        StVmm_ReleaseLocalPage(asp, vpn, count);
        return status;
    }

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "mapped %zu pages to %013zX\n", count, (uintptr_t)vpn);

    return STATUS_SUCCESS;
}

void StMm_UnmapGlobal(enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in)
{
    LOG_TRACE(LM_CAT_UNCLASSIFIED, "unmapping page %013zX (count=%zu)\n", (uintptr_t)vpn, count);

    StMmP_UnmapGlobalContiguousMemory(vpn, count);

    StVmm_ReleaseGlobalPage(domain, vpn, count);
}

void StMm_UnmapLocal(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
)
{
    if (!asp) return;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "unmapping page %013zX (count=%zu)\n", (uintptr_t)vpn, count);

    StMmP_UnmapLocalContiguousMemory(asp, vpn, count);

    StVmm_ReleaseLocalPage(asp, vpn, count);
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
    int vpn_reserved = 0;

    status = StPmm_AllocateContiguousFrame(&allocated_pfn, count, owner, alloc_flags);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StVmm_ReserveGlobalPage(
        domain,
        &allocated_vpn,
        count,
        owner,
        alloc_flags | AF_VMM_RESERVATION_SPARSE,
        map_flags
    );
    if (!CHECK_SUCCESS(status)) goto has_error;
    vpn_reserved = 1;

    status = StMmP_MapGlobalContiguousMemory(allocated_pfn, allocated_vpn, count, map_flags);
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
    if (vpn_reserved) {
        StVmm_ReleaseGlobalPage(domain, allocated_vpn, count);
    }

    if (allocated_pfn != (St_PhysFrame)-1) {
        StPmm_FreeContiguousFrame(allocated_pfn);
    }

    return status;
}

StStatus StMm_AllocateLocalContiguous(
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
    StAllocationOwner_StrongRef owner;
    int vpn_reserved = 0;

    if (!asp) return STATUS_INVALID_VALUE;

    owner = asp->process->alloc_owner;

    status = StPmm_AllocateContiguousFrame(&allocated_pfn, count, owner, alloc_flags);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StVmm_ReserveLocalPage(asp, &allocated_vpn, count, alloc_flags, map_flags);
    if (!CHECK_SUCCESS(status)) goto has_error;
    vpn_reserved = 1;

    status = StMmP_MapLocalContiguousMemory(asp, allocated_pfn, allocated_vpn, count, map_flags);
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
    if (vpn_reserved) {
        StVmm_ReleaseLocalPage(asp, allocated_vpn, count);
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

    /* reserve virtual memory pages first */
    status = StVmm_ReserveGlobalPage(domain, &allocated_vpn, count, owner, alloc_flags, map_flags);
    if (!CHECK_SUCCESS(status)) {
        LOG_ERROR(
            LM_CAT_UNCLASSIFIED,
            "StMm_AllocateGlobalSparse: StVmm_ReserveGlobalPage failed (domain=%d count=%zu "
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
        StVmm_ReleaseGlobalPage(domain, allocated_vpn, count);
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
        status = StVmm_ReserveLocalPage(
            asp,
            &allocated_vpn,
            count,
            alloc_flags | AF_VMM_RESERVATION_SPARSE | AF_VMM_RESERVATION_ON_DEMAND,
            map_flags
        );
        if (!CHECK_SUCCESS(status)) return status;

        status = StMmP_MapLocalSparseMemory(asp, allocated_vpn, count, map_flags);
        if (!CHECK_SUCCESS(status)) {
            StMmP_UnmapLocalSparseMemory(asp, allocated_vpn, count);
            StVmm_ReleaseLocalPage(asp, allocated_vpn, count);
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

    status = StVmm_ReserveLocalPage(
        asp,
        &allocated_vpn,
        count,
        alloc_flags | AF_VMM_RESERVATION_SPARSE,
        map_flags
    );
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
        StVmm_ReleaseLocalPage(asp, allocated_vpn, count);
    }

    return status;
}

StStatus StMm_AllocateGlobalContiguousTo(
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
    int vpn_reserved = 0;

    if (!IS_GLOBAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    status = StPmm_AllocateContiguousFrame(&allocated_pfn, count, owner, alloc_flags);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StVmm_ReserveGlobalPageTo(
        domain,
        vpn,
        count,
        owner,
        alloc_flags | AF_VMM_RESERVATION_SPARSE,
        map_flags
    );
    if (!CHECK_SUCCESS(status)) goto has_error;
    vpn_reserved = 1;

    status = StMmP_MapGlobalContiguousMemory(allocated_pfn, vpn, count, map_flags);
    if (!CHECK_SUCCESS(status)) goto has_error;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "allocated %zu pages to %013zX\n", count, (uintptr_t)vpn);

    return STATUS_SUCCESS;

has_error:
    if (vpn_reserved) {
        StVmm_ReleaseGlobalPage(domain, vpn, count);
    }

    if (allocated_pfn != (St_PhysFrame)-1) {
        StPmm_FreeContiguousFrame(allocated_pfn);
    }

    return status;
}

StStatus StMm_AllocateLocalContiguousTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    StStatus status;
    St_PhysFrame allocated_pfn = (St_PhysFrame)-1;
    StAllocationOwner_StrongRef owner;
    int vpn_reserved = 0;

    if (!asp) return STATUS_INVALID_VALUE;
    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;

    owner = asp->process->alloc_owner;

    status = StPmm_AllocateContiguousFrame(&allocated_pfn, count, owner, alloc_flags);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StVmm_ReserveLocalPageTo(asp, vpn, count, alloc_flags, map_flags);
    if (!CHECK_SUCCESS(status)) goto has_error;
    vpn_reserved = 1;

    status = StMmP_MapLocalContiguousMemory(asp, allocated_pfn, vpn, count, map_flags);
    if (!CHECK_SUCCESS(status)) goto has_error;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "allocated %zu pages to %013zX\n", count, (uintptr_t)vpn);

    return STATUS_SUCCESS;

has_error:
    if (vpn_reserved) {
        StVmm_ReleaseLocalPage(asp, vpn, count);
    }

    if (allocated_pfn != (St_PhysFrame)-1) {
        StPmm_FreeContiguousFrame(allocated_pfn);
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

    status = StVmm_ReserveGlobalPageTo(domain, vpn, count, owner, alloc_flags, map_flags);
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
        StVmm_ReleaseGlobalPage(domain, vpn, count);
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
        status = StVmm_ReserveLocalPageTo(
            asp,
            vpn,
            count,
            alloc_flags | AF_VMM_RESERVATION_SPARSE | AF_VMM_RESERVATION_ON_DEMAND,
            map_flags
        );
        if (!CHECK_SUCCESS(status)) return status;

        status = StMmP_MapLocalSparseMemory(asp, vpn, count, map_flags);
        if (!CHECK_SUCCESS(status)) {
            StMmP_UnmapLocalSparseMemory(asp, vpn, count);
            StVmm_ReleaseLocalPage(asp, vpn, count);
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

    status = StVmm_ReserveLocalPageTo(
        asp,
        vpn,
        count,
        alloc_flags | AF_VMM_RESERVATION_SPARSE,
        map_flags
    );
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
        StVmm_ReleaseLocalPage(asp, vpn, count);
    }

    return status;
}

StStatus StMm_AllocateLocalImageTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    const struct StMm_ImageBacking *image_backing __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    assert(image_backing);

    StStatus status;

    if (!asp) return STATUS_INVALID_VALUE;
    if (!IS_LOCAL_VPN(vpn)) return STATUS_INVALID_VALUE;
    if (map_flags & MF_IMMEDIATE) return STATUS_INVALID_VALUE;

    status = validate_image_backing(vpn, count, image_backing);
    if (!CHECK_SUCCESS(status)) return status;

    status = StVmm_ReserveLocalImagePageTo(
        asp,
        vpn,
        count,
        image_backing,
        alloc_flags | AF_VMM_RESERVATION_SPARSE,
        map_flags
    );
    if (!CHECK_SUCCESS(status)) return status;

    status = StMmP_MapLocalSparseMemory(asp, vpn, count, map_flags);
    if (!CHECK_SUCCESS(status)) {
        StMmP_UnmapLocalSparseMemory(asp, vpn, count);
        StVmm_ReleaseLocalPage(asp, vpn, count);
        return status;
    }

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "allocated %zu image pages to %013zX\n", count, (uintptr_t)vpn);

    return STATUS_SUCCESS;
}

StStatus StMm_ReserveGlobalContiguous(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    return StVmm_ReserveGlobalPage(domain, vpn, count, owner, alloc_flags, map_flags);
}

StStatus StMm_ReserveLocalContiguous(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    return StVmm_ReserveLocalPage(asp, vpn, count, alloc_flags, map_flags);
}

StStatus StMm_ReserveGlobalSparse(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    return StVmm_ReserveGlobalPage(
        domain,
        vpn,
        count,
        owner,
        alloc_flags | AF_VMM_RESERVATION_SPARSE,
        map_flags
    );
}

StStatus StMm_ReserveLocalSparse(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    return StVmm_ReserveLocalPage(
        asp,
        vpn,
        count,
        alloc_flags | AF_VMM_RESERVATION_SPARSE,
        map_flags
    );
}

StStatus StMm_ReserveGlobalContiguousTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    return StVmm_ReserveGlobalPageTo(domain, vpn, count, owner, alloc_flags, map_flags);
}

StStatus StMm_ReserveLocalContiguousTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    return StVmm_ReserveLocalPageTo(asp, vpn, count, alloc_flags, map_flags);
}

StStatus StMm_ReserveGlobalSparseTo(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    return StVmm_ReserveGlobalPageTo(
        domain,
        vpn,
        count,
        owner,
        alloc_flags | AF_VMM_RESERVATION_SPARSE,
        map_flags
    );
}

StStatus StMm_ReserveLocalSparseTo(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    StMm_AllocFlags alloc_flags __in,
    StMm_MapFlags map_flags __in
)
{
    return StVmm_ReserveLocalPageTo(
        asp,
        vpn,
        count,
        alloc_flags | AF_VMM_RESERVATION_SPARSE,
        map_flags
    );
}

static StStatus validate_global_commit_range(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    struct StVmm_PageInfo *page_info __out
)
{
    assert(page_info);

    StStatus status;
    St_VirtPage begin_vpn;
    St_VirtPage end_vpn;

    if (count == 0) return STATUS_INVALID_VALUE;

    status = StVmm_GetGlobalReservedRange(domain, vpn, &begin_vpn, &end_vpn);
    if (!CHECK_SUCCESS(status)) return status;
    if (vpn < begin_vpn || (St_PageCount)(end_vpn - vpn) < count) return STATUS_INVALID_VALUE;

    status = StVmm_GetGlobalPageInfo(domain, vpn, page_info);
    if (!CHECK_SUCCESS(status)) return status;
    if (page_info->alloc_flags & AF_VMM_RESERVATION_MAP) return STATUS_NOT_PERMITTED;
    if (page_info->mapping_policy.type != VMM_PAGE_MAPPING_PHYSICAL) {
        return STATUS_NOT_PERMITTED;
    }

    return STATUS_SUCCESS;
}

static StStatus validate_local_commit_range(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    struct StVmm_PageInfo *page_info __out
)
{
    assert(page_info);

    StStatus status;
    St_VirtPage begin_vpn;
    St_VirtPage end_vpn;

    if (!asp) return STATUS_INVALID_VALUE;
    if (count == 0) return STATUS_INVALID_VALUE;

    status = StVmm_GetLocalReservedRange(asp, vpn, &begin_vpn, &end_vpn);
    if (!CHECK_SUCCESS(status)) return status;
    if (vpn < begin_vpn || (St_PageCount)(end_vpn - vpn) < count) return STATUS_INVALID_VALUE;

    status = StVmm_GetLocalPageInfo(asp, vpn, page_info);
    if (!CHECK_SUCCESS(status)) return status;
    if (page_info->alloc_flags & AF_VMM_RESERVATION_MAP) return STATUS_NOT_PERMITTED;
    if (page_info->mapping_policy.type != VMM_PAGE_MAPPING_PHYSICAL) {
        return STATUS_NOT_PERMITTED;
    }

    return STATUS_SUCCESS;
}

static StStatus commit_global_contiguous(
    enum StVmm_Domain domain __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    const struct StVmm_PageInfo *page_info __in
)
{
    assert(page_info);

    StStatus status;
    St_PhysFrame pfn = (St_PhysFrame)-1;
    StMm_AllocFlags alloc_flags = page_info->alloc_flags & ~AF_VMM_RESERVATION_MASK;

    for (St_PageCount i = 0; i < count; i++) {
        status = StMm_GlobalVirtPageToPhysFrame(vpn + i, NULL);
        if (CHECK_SUCCESS(status)) return STATUS_DUPLICATE_ENTRY;
        if (status != STATUS_PAGE_NOT_PRESENT) return status;
    }

    status = StPmm_AllocateContiguousFrame(&pfn, count, page_info->owner, alloc_flags);
    if (!CHECK_SUCCESS(status)) return status;

    status = StMmP_MapGlobalContiguousMemory(pfn, vpn, count, page_info->map_flags);
    if (!CHECK_SUCCESS(status)) {
        StPmm_FreeContiguousFrame(pfn);
        return status;
    }

    (void)domain;
    return STATUS_SUCCESS;
}

static StStatus commit_local_contiguous(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    const struct StVmm_PageInfo *page_info __in
)
{
    assert(page_info);

    StStatus status;
    St_PhysFrame pfn = (St_PhysFrame)-1;
    StMm_AllocFlags alloc_flags = page_info->alloc_flags & ~AF_VMM_RESERVATION_MASK;

    for (St_PageCount i = 0; i < count; i++) {
        status = StMm_LocalVirtPageToPhysFrame(asp, vpn + i, NULL);
        if (CHECK_SUCCESS(status)) return STATUS_DUPLICATE_ENTRY;
        if (status != STATUS_PAGE_NOT_PRESENT) return status;
    }

    status = StPmm_AllocateContiguousFrame(&pfn, count, page_info->owner, alloc_flags);
    if (!CHECK_SUCCESS(status)) return status;

    status = StMmP_MapLocalContiguousMemory(asp, pfn, vpn, count, page_info->map_flags);
    if (!CHECK_SUCCESS(status)) {
        StPmm_FreeContiguousFrame(pfn);
        return status;
    }

    return STATUS_SUCCESS;
}

static StStatus commit_global_sparse(
    St_VirtPage vpn __in, St_PageCount count __in, const struct StVmm_PageInfo *page_info __in
)
{
    assert(page_info);

    StStatus status;
    St_PhysFrame pfn = (St_PhysFrame)-1;
    St_PageCount committed_count = 0;
    St_PageCount batch_count = 0;
    StMm_AllocFlags alloc_flags = page_info->alloc_flags & ~AF_VMM_RESERVATION_MASK;

    while (committed_count < count) {
        batch_count = get_sparse_alloc_batch_count(count - committed_count);

        for (St_PageCount i = 0; i < batch_count; i++) {
            status = StMm_GlobalVirtPageToPhysFrame(vpn + committed_count + i, NULL);
            if (CHECK_SUCCESS(status)) {
                status = STATUS_DUPLICATE_ENTRY;
                goto has_error;
            }
            if (status != STATUS_PAGE_NOT_PRESENT) goto has_error;
        }

        status = allocate_sparse_frame_batch(
            &pfn,
            &batch_count,
            count - committed_count,
            page_info->owner,
            alloc_flags
        );
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = StMmP_MapGlobalContiguousMemory(
            pfn,
            vpn + committed_count,
            batch_count,
            page_info->map_flags
        );
        if (!CHECK_SUCCESS(status)) {
            StPmm_FreeContiguousFrame(pfn);
            goto has_error;
        }

        committed_count += batch_count;
    }

    return STATUS_SUCCESS;

has_error:
    if (committed_count > 0) {
        rollback_global_sparse_allocation(vpn, committed_count);
        StMmP_UnmapGlobalContiguousMemory(vpn, committed_count);
    }
    return status;
}

static StStatus commit_local_sparse(
    StAddressSpace_StrongRef asp __in,
    St_VirtPage vpn __in,
    St_PageCount count __in,
    const struct StVmm_PageInfo *page_info __in
)
{
    assert(page_info);

    StStatus status;
    St_PhysFrame pfn = (St_PhysFrame)-1;
    St_PageCount committed_count = 0;
    St_PageCount batch_count = 0;
    StMm_AllocFlags alloc_flags = page_info->alloc_flags & ~AF_VMM_RESERVATION_MASK;

    while (committed_count < count) {
        batch_count = get_sparse_alloc_batch_count(count - committed_count);

        for (St_PageCount i = 0; i < batch_count; i++) {
            status = StMm_LocalVirtPageToPhysFrame(asp, vpn + committed_count + i, NULL);
            if (CHECK_SUCCESS(status)) {
                status = STATUS_DUPLICATE_ENTRY;
                goto has_error;
            }
            if (status != STATUS_PAGE_NOT_PRESENT) goto has_error;
        }

        status = allocate_sparse_frame_batch(
            &pfn,
            &batch_count,
            count - committed_count,
            page_info->owner,
            alloc_flags
        );
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = StMmP_MapLocalContiguousMemory(
            asp,
            pfn,
            vpn + committed_count,
            batch_count,
            page_info->map_flags
        );
        if (!CHECK_SUCCESS(status)) {
            StPmm_FreeContiguousFrame(pfn);
            goto has_error;
        }

        committed_count += batch_count;
    }

    return STATUS_SUCCESS;

has_error:
    if (committed_count > 0) {
        rollback_local_sparse_allocation(asp, vpn, committed_count);
        StMmP_UnmapLocalContiguousMemory(asp, vpn, committed_count);
    }
    return status;
}

StStatus StMm_CommitGlobal(
    enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in
)
{
    StStatus status;
    struct StVmm_PageInfo page_info;

    status = validate_global_commit_range(domain, vpn, count, &page_info);
    if (!CHECK_SUCCESS(status)) return status;

    if (page_info.physical_layout == VMM_PHYSICAL_LAYOUT_CONTIGUOUS) {
        return commit_global_contiguous(domain, vpn, count, &page_info);
    }

    return commit_global_sparse(vpn, count, &page_info);
}

StStatus StMm_CommitLocal(
    StAddressSpace_StrongRef asp __in, St_VirtPage vpn __in, St_PageCount count __in
)
{
    StStatus status;
    struct StVmm_PageInfo page_info;

    status = validate_local_commit_range(asp, vpn, count, &page_info);
    if (!CHECK_SUCCESS(status)) return status;

    if (page_info.physical_layout == VMM_PHYSICAL_LAYOUT_CONTIGUOUS) {
        return commit_local_contiguous(asp, vpn, count, &page_info);
    }

    return commit_local_sparse(asp, vpn, count, &page_info);
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

    status = StVmm_ResolveLocalPage(asp, vpn);
    if (!CHECK_SUCCESS(status)) return status;

    status = StVmm_GetLocalPageInfo(asp, vpn, &page_info);
    if (!CHECK_SUCCESS(status)) return status;
    if (page_info.mapping_policy.type != VMM_PAGE_MAPPING_DEMAND_ZERO &&
        page_info.mapping_policy.type != VMM_PAGE_MAPPING_IMAGE) {
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
        page_info.alloc_flags & ~AF_VMM_RESERVATION_MASK
    );
    if (!CHECK_SUCCESS(status)) return status;

    status = StMmP_MapLocalContiguousMemory(asp, pfn, vpn, (St_PageCount)1, page_info.map_flags);
    if (!CHECK_SUCCESS(status)) {
        StPmm_FreeContiguousFrame(pfn);
        return status;
    }

    if (page_info.mapping_policy.type == VMM_PAGE_MAPPING_IMAGE) {
        status = fill_image_backed_frame(pfn, vpn, &page_info.mapping_policy.image);
        if (!CHECK_SUCCESS(status)) {
            StMmP_UnmapLocalContiguousMemory(asp, vpn, (St_PageCount)1);
            StPmm_FreeContiguousFrame(pfn);
            return status;
        }
    }

    return STATUS_SUCCESS;
}

void StMm_FreeGlobal(enum StVmm_Domain domain __in, St_VirtPage vpn __in, St_PageCount count __in)
{
    StStatus status;
    St_PhysFrame pfn;
    StPmm_AllocationMetadata_BorrowedRef metadata;
    struct StVmm_PageInfo page_info;
    size_t page_count_to_free;
    size_t i = 0;
    int allow_unmapped_pages = 0;

    if (!IS_GLOBAL_VPN(vpn)) return;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "freeing %zu pages at %013zX\n", count, (uintptr_t)vpn);

    status = StVmm_GetGlobalPageInfo(domain, vpn, &page_info);
    if (CHECK_SUCCESS(status) && !(page_info.alloc_flags & AF_VMM_RESERVATION_MAP) &&
        page_info.mapping_policy.type == VMM_PAGE_MAPPING_PHYSICAL) {
        allow_unmapped_pages = 1;
    }

    while (i < count) {
        status = StMm_GlobalVirtPageToPhysFrame(vpn + i, &pfn);
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
        if (allow_unmapped_pages) {
            StMmP_UnmapGlobalContiguousMemory(vpn + i, (St_PageCount)page_count_to_free);
        }

        i += page_count_to_free;
    }

    if (!allow_unmapped_pages) {
        StMmP_UnmapGlobalContiguousMemory(vpn, count);
    }

    StVmm_ReleaseGlobalPage(domain, vpn, count);
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
    St_VirtPage free_vpn = vpn;
    St_PageCount free_count = count;
    St_VirtPage reserved_begin_vpn;
    St_VirtPage reserved_end_vpn;
    size_t i = 0;
    int allow_unmapped_pages = 0;

    if (!IS_LOCAL_VPN(vpn)) return;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "freeing %zu pages at %013zX\n", count, (uintptr_t)vpn);

    status = StVmm_GetLocalPageInfo(asp, vpn, &page_info);
    if (CHECK_SUCCESS(status) &&
        (page_info.mapping_policy.type == VMM_PAGE_MAPPING_DEMAND_ZERO ||
         page_info.mapping_policy.type == VMM_PAGE_MAPPING_IMAGE ||
         page_info.mapping_policy.type == VMM_PAGE_MAPPING_GUARD ||
         (!(page_info.alloc_flags & AF_VMM_RESERVATION_MAP) &&
          page_info.mapping_policy.type == VMM_PAGE_MAPPING_PHYSICAL))) {
        allow_unmapped_pages = 1;
    }

    if (CHECK_SUCCESS(status) && allow_unmapped_pages && (page_info.map_flags & MF_GUARD)) {
        status = StVmm_GetLocalReservedRange(asp, vpn, &reserved_begin_vpn, &reserved_end_vpn);
        if (!CHECK_SUCCESS(status) || reserved_end_vpn <= reserved_begin_vpn) {
            St_Panic(STATUS_CONFLICTING_STATE, "guarded local VMM range unavailable");
        }

        free_vpn = reserved_begin_vpn;
        free_count = (St_PageCount)(reserved_end_vpn - reserved_begin_vpn);
    }

    while (i < free_count) {
        status = StMm_LocalVirtPageToPhysFrame(asp, free_vpn + i, &pfn);
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
        if (page_count_to_free > free_count - i) page_count_to_free = free_count - i;
        StPmm_FreeContiguousFrame(pfn);
        StMmP_UnmapLocalContiguousMemory(asp, free_vpn + i, (St_PageCount)page_count_to_free);

        i += page_count_to_free;
    }

    if (allow_unmapped_pages) {
        StMmP_UnmapLocalSparseMemory(asp, free_vpn, free_count);
    }

    StVmm_ReleaseLocalPage(asp, free_vpn, free_count);
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

    status = StVmm_SetLocalPageFlags(asp, vpn, count, mapflags);
    if (!CHECK_SUCCESS(status)) return status;

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
    struct vmm_reservation_node *node, *next;

    if (!owner) return;

    StRefControlBlock_MarkDying(&owner->ref_control);

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "cleaning up owner allocations\n");

    node = owner->first_vmm_reservation;
    while (node) {
        St_VirtPage usable_vpn;
        St_PageCount usable_count;

        next = node->owner_next;
        usable_vpn = node->base_vpn + (St_VirtPage)node->guard_page_count;
        usable_count = (St_PageCount)(node->limit_vpn - usable_vpn);

        if (node->reservation_type == VMM_RESERVATION_ALLOC) {
            if (node->asp) {
                StMm_FreeLocal((StAddressSpace_StrongRef)node->asp, usable_vpn, usable_count);
            } else {
                StMm_FreeGlobal(node->domain, usable_vpn, usable_count);
            }
        } else {
            if (node->asp) {
                StMm_UnmapLocal((StAddressSpace_StrongRef)node->asp, usable_vpn, usable_count);
            } else {
                StMm_UnmapGlobal(node->domain, usable_vpn, usable_count);
            }
        }
        node = next;
    }
}
