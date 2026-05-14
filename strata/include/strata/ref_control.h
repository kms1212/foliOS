#ifndef __STRATA_REF_CONTROL_H__
#define __STRATA_REF_CONTROL_H__

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>

#include <strata/compiler.h>
#include <strata/mm/types.h>

#define REFCTL_BLOCK_DYING       ((uint32_t)0x00000001)
#define REFCTL_BLOCK_REAP_QUEUED ((uint32_t)0x00000002)

typedef void (*StRefControlBlock_FinalizeFunc)(void *object __in);

struct StRefControlBlock {
    atomic_uint_fast32_t ref_count;
    uint32_t flags;
    St_PageCount deferred_reap_page_count;
    void *object;
    StRefControlBlock_FinalizeFunc finalize;
};

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

static inline void StRefControlBlock_Acquire(struct StRefControlBlock *ref_control __inout)
{
    assert(ref_control);

    atomic_fetch_add_explicit(&ref_control->ref_count, 1, memory_order_relaxed);
}

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

static inline int StRefControlBlock_IsDying(const struct StRefControlBlock *ref_control __in)
{
    assert(ref_control);

    return (ref_control->flags & REFCTL_BLOCK_DYING) != 0;
}

static inline void StRefControlBlock_MarkDying(struct StRefControlBlock *ref_control __inout)
{
    assert(ref_control);

    ref_control->flags |= REFCTL_BLOCK_DYING;
}

static inline int StRefControlBlock_IsReapQueued(const struct StRefControlBlock *ref_control __in)
{
    assert(ref_control);

    return (ref_control->flags & REFCTL_BLOCK_REAP_QUEUED) != 0;
}

static inline void StRefControlBlock_MarkReapQueued(struct StRefControlBlock *ref_control __inout)
{
    assert(ref_control);

    ref_control->flags |= REFCTL_BLOCK_REAP_QUEUED;
}

static inline void StRefControlBlock_ClearReapQueued(struct StRefControlBlock *ref_control __inout)
{
    assert(ref_control);

    ref_control->flags &= ~REFCTL_BLOCK_REAP_QUEUED;
}

#endif  // __STRATA_REF_CONTROL_H__
