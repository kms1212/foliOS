#ifndef __STRATA_MM_ALLOCATION_OWNER_H__
#define __STRATA_MM_ALLOCATION_OWNER_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/mm/allocation_owner_refs.h>
#include <strata/ref_control.h>
#include <strata/status.h>
#include <strata/types.h>

#include <strata/mm/types.h>

/**
 * Ref-counted memory allocation owner.
 *
 * The owner records VMM reservations and usage accounting for a process or
 * subsystem. Closing an owner walks the reservation list and releases associated
 * mappings/allocations through MM lifecycle APIs.
 */
struct StAllocationOwner {
    /** First-field ref control block used by StAllocationOwner_Acquire/Release. */
    struct StRefControlBlock ref_control;

    /** Intrusive VMM reservation list owned by this allocation owner. */
    void *first_vmm_reservation;
    void *last_vmm_reservation;
    /** Current charged page/frame usage. */
    St_PageCount page_usage_count;
    /** Peak charged page/frame usage for diagnostics. */
    St_PageCount page_usage_peak_count;
};

/** Create an allocation owner with one strong reference. */
StStatus StAllocationOwner_Create(StAllocationOwner_StrongRef *owner __out);
/** Mark owner closed and release all owner-linked reservations. */
void StAllocationOwner_Close(StAllocationOwner_StrongRef owner __in);
/** Acquire another strong owner reference. */
void StAllocationOwner_Acquire(StAllocationOwner_StrongRef owner __inout);
/** Release a strong owner reference. */
void StAllocationOwner_Release(StAllocationOwner_StrongRef owner __inout);
/** Return nonzero once the owner is closed to new reservations. */
int StAllocationOwner_IsClosed(StAllocationOwner_StrongRef owner __in);

#endif  // __STRATA_MM_ALLOCATION_OWNER_H__
