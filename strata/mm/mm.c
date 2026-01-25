#include <strata/mm.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <strata/arch/mmu.h>

#include <strata/plat/mmu.h>

#include <strata/macros.h>
#include <strata/panic.h>
#include <strata/log.h>

#define MODULE_NAME "mm"

StStatus StMm_Init(void)
{
    return STATUS_SUCCESS;
}

StStatus StMm_VirtPageToPhysFrame(
    St_VirtPage vpn __in,
    St_PhysFrame *pfn __out_optional
)
{
    return StMmuP_VirtPageToPhysFrame(vpn, pfn);
}

StStatus StMm_MapContiguous(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PhysFrame pfn __in,
    St_PageCount count __in,
    StVmm_AllocFlags vmmflags __in,
    StMm_MapFlags mapflags __in
)
{
    StStatus status;
    St_VirtPage allocated_vpn = (St_VirtPage)-1;

    status = StVmm_AllocatePage(domain, &allocated_vpn, count, vmmflags);
    if (!CHECK_SUCCESS(status)) return status;

    *vpn = allocated_vpn;

    for (size_t i = 0; i < count; i++) {
        status = StMmuP_MapMemory(pfn + i, allocated_vpn + i, mapflags);
        if (!CHECK_SUCCESS(status)) return status;
    }

    LOG_TRACE("mapped page %lu-%lu to frame %lu-%lu\n", (uintptr_t)allocated_vpn, (uintptr_t)allocated_vpn + (uintptr_t)count - 1, (uintptr_t)pfn, (uintptr_t)pfn + (uintptr_t)count - 1);

    return STATUS_SUCCESS;
}

void StMm_UnmapContiguous(
    St_VirtPage vpn __in,
    St_PageCount count __in
)
{
    LOG_TRACE("unmapping page %lu-%lu\n", (uintptr_t)vpn, (uintptr_t)vpn + (uintptr_t)count - 1);

    for (size_t i = 0; i < count; i++) {
        StMmuP_UnmapMemory(vpn + i);
    }

    StVmm_FreePage(vpn, count);
}

StStatus StMm_AllocateContiguous(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StPmm_AllocFlags pmmflags __in,
    StVmm_AllocFlags vmmflags __in,
    StMm_MapFlags mapflags __in
)
{
    StStatus status;
    St_VirtPage allocated_vpn = (St_VirtPage)-1;
    St_PhysFrame allocated_pfn = (St_PhysFrame)-1;

    status = StPmm_AllocateContiguousFrame(&allocated_pfn, count, pmmflags);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StMm_MapContiguous(domain, &allocated_vpn, allocated_pfn, count, vmmflags, mapflags);
    if (!CHECK_SUCCESS(status)) goto has_error;

    *vpn = allocated_vpn;

    return STATUS_SUCCESS;

has_error:
    if (allocated_vpn != (St_VirtPage)-1) {
        StMm_UnmapContiguous(allocated_vpn, count);
    }

    if (allocated_pfn != (St_PhysFrame)-1) {
        StPmm_FreeContiguousFrame(allocated_pfn);
    }

    return status;
}

// TODO: apply adaptive batch decay
StStatus StMm_AllocateSparse(
    enum StVmm_Domain domain __in,
    St_VirtPage *vpn __out,
    St_PageCount count __in,
    StPmm_AllocFlags pmmflags __in,
    StVmm_AllocFlags vmmflags __in,
    StMm_MapFlags mapflags __in
)
{
    StStatus status;
    St_PhysFrame allocated_pfn = (St_PhysFrame)-1;
    St_VirtPage allocated_vpn = (St_VirtPage)-1;
    size_t allocated_count = 0;

    pmmflags &= ~PMM_ALIGN_MASK;
    
    /* allocate virtual memory pages first */
    status = StVmm_AllocatePage(domain, &allocated_vpn, count, vmmflags);
    if (!CHECK_SUCCESS(status)) goto has_error;

    /* it will allow allocating non-contiguous physical memory frames */
    /* if you want to allocate/map frames/pages for hardware I/O, */
    /* you should use StPmm_AllocateContiguous() */
    for (; allocated_count < count; allocated_count++) {
        status = StMm_VirtPageToPhysFrame(allocated_vpn + (St_VirtPage)allocated_count, NULL);
        if (status != STATUS_PAGE_NOT_PRESENT) goto has_error;

        status = StPmm_AllocateContiguousFrame(&allocated_pfn, (St_PageCount)1, pmmflags);
        if (!CHECK_SUCCESS(status)) goto has_error;

        status = StMmuP_MapMemory(allocated_pfn, allocated_vpn + (St_VirtPage)allocated_count, mapflags);
        if (!CHECK_SUCCESS(status)) {
            StPmm_FreeContiguousFrame(allocated_pfn);
            goto has_error;
        }
    }

    *vpn = allocated_vpn;

    return STATUS_SUCCESS;

has_error:
    /* allocation failed. rollback changes */
    for (size_t i = 0; i < allocated_count; i++) {
        /* read vpn-pfn mapping to know which frames to free */
        if (!CHECK_SUCCESS(StMm_VirtPageToPhysFrame(allocated_vpn + i, &allocated_pfn))) continue;

        /* unmap vpn & free frame*/
        StMmuP_UnmapMemory(allocated_vpn + i);
        StPmm_FreeContiguousFrame(allocated_pfn);
    }

    if (allocated_vpn != (St_VirtPage)-1) {
        StVmm_FreePage(allocated_vpn, count);
    }

    return status;
}

void StMm_Free(
    St_VirtPage vpn __in,
    St_PageCount count __in
)
{
    StStatus status;
    St_PhysFrame pfn;
    struct StPmm_AllocationMetadata *metadata;
    size_t i = 0;

    while (i < count) {
        status = StMm_VirtPageToPhysFrame(vpn + i, &pfn);
        if (!CHECK_SUCCESS(status)) {
            St_Panic(STATUS_CONFLICTING_STATE, "tried to free unmapped page");
        }
        
        status = StPmm_GetAllocMetadata(pfn, &metadata);
        if (!CHECK_SUCCESS(status)) {
            St_Panic(STATUS_CONFLICTING_STATE, "pmm allocation metadata unavailable");
        }
    
        StMmuP_UnmapMemory(vpn + i);
        StPmm_FreeContiguousFrame(pfn);

        i += 1 << metadata->order;
    }

    StVmm_FreePage(vpn, count);
}
