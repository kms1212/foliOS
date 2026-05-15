#ifndef __STRATA_REF_CONTROL_H__
#define __STRATA_REF_CONTROL_H__

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>

#include <strata/compiler.h>
#include <strata/mm/types.h>

#define REFCTL_BLOCK_DYING       ((uint32_t)0x00000001)
#define REFCTL_BLOCK_REAP_QUEUED ((uint32_t)0x00000002)

/** Finalizer called when the last strong reference is released. */
typedef void (*StRefControlBlock_FinalizeFunc)(void *object __in);

/**
 * Intrusive reference-control block for first-class Strata objects.
 *
 * Ref-counted object bodies embed this as their first field. Object-specific
 * Acquire/Release wrappers should be used outside the owning implementation so
 * reference meaning remains visible in the API.
 */
struct StRefControlBlock {
    /** Strong reference count. */
    atomic_uint_fast32_t ref_count;
    /** REFCTL_BLOCK_* lifetime/reap flags. */
    uint32_t flags;
    /** Page cleanup budget recorded for deferred reap paths. */
    St_PageCount deferred_reap_page_count;
    /** Owning object passed back to finalize. */
    void *object;
    /** Optional finalizer run on last release. */
    StRefControlBlock_FinalizeFunc finalize;
};

/** Initialize an embedded reference-control block. */
static inline void StRefControlBlock_Init(
    struct StRefControlBlock *ref_control __out,
    uint32_t ref_count __in,
    void *object __in,
    StRefControlBlock_FinalizeFunc finalize __in
)
{
    assert(ref_control);

    atomic_init(&ref_control->ref_count, ref_count);
    ref_control->flags = 0;
    ref_control->deferred_reap_page_count = 0;
    ref_control->object = object;
    ref_control->finalize = finalize;
}

/** Increment the strong reference count. */
static inline void StRefControlBlock_Acquire(struct StRefControlBlock *ref_control __inout)
{
    assert(ref_control);

    atomic_fetch_add_explicit(&ref_control->ref_count, 1, memory_order_relaxed);
}

/** Release one strong reference and run finalize on the last release. */
static inline int StRefControlBlock_Release(struct StRefControlBlock *ref_control __inout)
{
    assert(ref_control);

    if (atomic_fetch_sub_explicit(&ref_control->ref_count, 1, memory_order_acq_rel) != 1) {
        return 0;
    }

    if (ref_control->finalize) {
        ref_control->finalize(ref_control->object);
    }

    return 1;
}

/** Return nonzero after public removal has marked the object dying. */
static inline int StRefControlBlock_IsDying(const struct StRefControlBlock *ref_control __in)
{
    assert(ref_control);

    return (ref_control->flags & REFCTL_BLOCK_DYING) != 0;
}

/** Mark an object dying so new public acquisition/lookup can reject it. */
static inline void StRefControlBlock_MarkDying(struct StRefControlBlock *ref_control __inout)
{
    assert(ref_control);

    ref_control->flags |= REFCTL_BLOCK_DYING;
}

/** Return nonzero if the object is already queued for deferred reap. */
static inline int StRefControlBlock_IsReapQueued(const struct StRefControlBlock *ref_control __in)
{
    assert(ref_control);

    return (ref_control->flags & REFCTL_BLOCK_REAP_QUEUED) != 0;
}

/** Mark an object queued for deferred reap. */
static inline void StRefControlBlock_MarkReapQueued(struct StRefControlBlock *ref_control __inout)
{
    assert(ref_control);

    ref_control->flags |= REFCTL_BLOCK_REAP_QUEUED;
}

/** Clear the deferred reap marker after the object is actually reaped. */
static inline void StRefControlBlock_ClearReapQueued(struct StRefControlBlock *ref_control __inout)
{
    assert(ref_control);

    ref_control->flags &= ~REFCTL_BLOCK_REAP_QUEUED;
}

#endif  // __STRATA_REF_CONTROL_H__
