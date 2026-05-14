#ifndef __STRATA_MM_PMM_H__
#define __STRATA_MM_PMM_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/status.h>
#include <strata/types.h>

#include <strata/mm/owner.h>
#include <strata/mm/types.h>

#ifndef __STRATA_PMM_ALLOCATION_METADATA_REFS_DEFINED__
#    define __STRATA_PMM_ALLOCATION_METADATA_REFS_DEFINED__
struct StPmm_AllocationMetadata;
typedef struct StPmm_AllocationMetadata *StPmm_AllocationMetadata_BorrowedRef __ref_borrowed;
typedef struct StPmm_AllocationMetadata *StPmm_AllocationMetadata_LockedRef __ref_locked;
#endif

struct StPmm_AllocationMetadata {
    const uint32_t order;
    uint32_t flags;
    StMm_AllocationOwner_StrongRef const owner;
};

StStatus StPmm_Init(void);
StStatus StPmm_LateInit(void);

void StPmm_GetTotalFrameCount(St_PageCount *frame_count __out);
void StPmm_GetFreeFrameCount(St_PageCount *count __out);

StStatus StPmm_AllocateContiguousFrame(
    St_PhysFrame *pfn __out,
    St_PageCount count __in,
    StMm_AllocationOwner_StrongRef owner __in,
    StMm_AllocFlags alloc_flags __in
);
StStatus StPmm_AcquireContiguousFrame(St_PhysFrame pfn __in);
void StPmm_FreeContiguousFrame(St_PhysFrame pfn __in);

StStatus StPmm_MarkUsableContiguousFrame(St_PhysFrame base __in, St_PhysFrame limit __in);
StStatus StPmm_MarkUnusableContiguousFrame(St_PhysFrame base __in, St_PhysFrame limit __in);

StStatus StPmm_GetAllocMetadata(
    St_PhysFrame pfn __in, StPmm_AllocationMetadata_BorrowedRef *meta __out
);
StStatus StPmm_LockAndGetAllocMetadata(
    St_PhysFrame pfn __in, StPmm_AllocationMetadata_LockedRef *meta __out
);
StStatus StPmm_UnlockAllocMetadata(StPmm_AllocationMetadata_LockedRef meta __in);

#ifdef TESTING
void StPmm_DebugDumpRegion(St_PhysFrame start_pfn __in, St_PageCount count __in);
void StPmm_DebugDumpAtpa(void);
#endif

#endif  // __STRATA_MM_PMM_H__
