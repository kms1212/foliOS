#ifndef __STRATA_MM_OWNER_H__
#define __STRATA_MM_OWNER_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/ref_control.h>
#include <strata/status.h>
#include <strata/types.h>

#include <strata/mm/types.h>

#ifndef __STRATA_MM_ALLOCATION_OWNER_REFS_DEFINED__
#    define __STRATA_MM_ALLOCATION_OWNER_REFS_DEFINED__
struct StMm_AllocationOwner;
typedef struct StMm_AllocationOwner *StMm_AllocationOwner_StrongRef __ref_strong;
typedef struct StMm_AllocationOwner *StMm_AllocationOwner_WeakRef __ref_weak;
typedef struct StMm_AllocationOwner *StMm_AllocationOwner_BorrowedRef __ref_borrowed;
typedef struct StMm_AllocationOwner *StMm_AllocationOwner_InternalRef __ref_internal;
#endif

struct StMm_AllocationOwner {
    struct StRefControlBlock ref_control;

    void *first_vmm_node;
    void *last_vmm_node;
    St_PageCount page_usage_count;
    St_PageCount page_usage_peak_count;
};

StStatus StMm_CreateAllocationOwner(StMm_AllocationOwner_StrongRef *owner __out);
void StMm_CloseAllocationOwner(StMm_AllocationOwner_StrongRef owner __in);
void StMm_AcquireAllocationOwner(StMm_AllocationOwner_StrongRef owner __inout);
void StMm_ReleaseAllocationOwner(StMm_AllocationOwner_StrongRef owner __inout);
int StMm_IsAllocationOwnerClosed(StMm_AllocationOwner_StrongRef owner __in);

#endif  // __STRATA_MM_OWNER_H__
