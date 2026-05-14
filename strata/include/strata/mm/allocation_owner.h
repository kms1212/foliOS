#ifndef __STRATA_MM_ALLOCATION_OWNER_H__
#define __STRATA_MM_ALLOCATION_OWNER_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/mm/allocation_owner_refs.h>
#include <strata/ref_control.h>
#include <strata/status.h>
#include <strata/types.h>

#include <strata/mm/types.h>

struct StAllocationOwner {
    struct StRefControlBlock ref_control;

    void *first_vmm_node;
    void *last_vmm_node;
    St_PageCount page_usage_count;
    St_PageCount page_usage_peak_count;
};

StStatus StAllocationOwner_Create(StAllocationOwner_StrongRef *owner __out);
void StAllocationOwner_Close(StAllocationOwner_StrongRef owner __in);
void StAllocationOwner_Acquire(StAllocationOwner_StrongRef owner __inout);
void StAllocationOwner_Release(StAllocationOwner_StrongRef owner __inout);
int StAllocationOwner_IsClosed(StAllocationOwner_StrongRef owner __in);

#endif  // __STRATA_MM_ALLOCATION_OWNER_H__
