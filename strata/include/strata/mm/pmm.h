#ifndef __STRATA_MM_PMM_H__
#define __STRATA_MM_PMM_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/status.h>
#include <strata/types.h>

#include <strata/mm/allocation_owner_refs.h>
#include <strata/mm/pmm_refs.h>
#include <strata/mm/types.h>

/**
 * Metadata recorded for an allocated physical frame run.
 *
 * Borrowed metadata is only an unlocked snapshot view. Locked metadata must be
 * released through StPmm_UnlockAllocMetadata before returning to callers that
 * may mutate allocator state.
 */
struct StPmm_AllocationMetadata {
    /** Allocation order used by the physical allocator. */
    const uint32_t order;
    /** PMM-private allocation flags. */
    uint32_t flags;
    /** Owner charged for the backing frame run. */
    StAllocationOwner_StrongRef const owner;
};

/** Initialize boot-time physical memory allocator state. */
StStatus StPmm_Init(void);
/** Finish PMM initialization after dependent allocators are available. */
StStatus StPmm_LateInit(void);

/** Write total managed physical frame count. */
void StPmm_GetTotalFrameCount(St_PageCount *frame_count __out);
/** Write currently free physical frame count. */
void StPmm_GetFreeFrameCount(St_PageCount *count __out);

/** Allocate a contiguous run of physical frames and charge owner. */
StStatus StPmm_AllocateContiguousFrame(
    St_PhysFrame *pfn __out,
    St_PageCount count __in,
    StAllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in
);
/** Retain an existing contiguous allocation by its base frame. */
StStatus StPmm_AcquireContiguousFrame(St_PhysFrame pfn __in);
/** Free a contiguous allocation by its base frame. */
void StPmm_FreeContiguousFrame(St_PhysFrame pfn __in);

/** Mark the inclusive frame range [base, limit] usable by PMM. */
StStatus StPmm_MarkUsableContiguousFrame(St_PhysFrame base __in, St_PhysFrame limit __in);
/** Mark the inclusive frame range [base, limit] unavailable to PMM. */
StStatus StPmm_MarkUnusableContiguousFrame(St_PhysFrame base __in, St_PhysFrame limit __in);

/** Return an unlocked borrowed allocation metadata view for pfn. */
StStatus StPmm_GetAllocMetadata(
    St_PhysFrame pfn __in, StPmm_AllocationMetadata_BorrowedRef *meta __out
);
/** Return a locked allocation metadata view for pfn. */
StStatus StPmm_LockAndGetAllocMetadata(
    St_PhysFrame pfn __in, StPmm_AllocationMetadata_LockedRef *meta __out
);
/** Release metadata returned by StPmm_LockAndGetAllocMetadata. */
StStatus StPmm_UnlockAllocMetadata(StPmm_AllocationMetadata_LockedRef meta __in);

#ifdef TESTING
void StPmm_DebugDumpRegion(St_PhysFrame start_pfn __in, St_PageCount count __in);
void StPmm_DebugDumpAtpa(void);
#endif

#endif  // __STRATA_MM_PMM_H__
