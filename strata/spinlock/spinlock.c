#include <strata/spinlock.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>

#include <strata/arch/interrupt.h>
#include <strata/arch/intrinsics/misc.h>

#include <strata/compiler.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>

void StSpinlock_Init(struct StSpinlock *lock __in)
{
    atomic_flag_clear(&lock->locked);
    lock->owner = NULL;
}

StStatus StSpinlock_Lock(struct StSpinlock *lock __in)
{
    StStatus status;
    StThread_InternalRef th;

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) return status;

    StThread_LockPreemption();

    while (atomic_flag_test_and_set_explicit(&lock->locked, memory_order_acquire)) {
        /* busy wait */
        StA_Pause();
    }

    lock->owner = th;

    return STATUS_SUCCESS;
}

StStatus StSpinlock_TryLock(struct StSpinlock *lock __in, int *locked __out)
{
    assert(locked);

    StStatus status;
    StThread_InternalRef th;

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) return status;

    StThread_LockPreemption();

    if (atomic_flag_test_and_set_explicit(&lock->locked, memory_order_acquire)) {
        StThread_UnlockPreemption();

        *locked = 0;
        return STATUS_SUCCESS;
    }

    lock->owner = th;

    *locked = 1;
    return STATUS_SUCCESS;
}

void StSpinlock_Unlock(struct StSpinlock *lock __in)
{
    StStatus status;
    StThread_InternalRef th;

    status = StScheduler_GetCurrentThread(&th);
    assert(CHECK_SUCCESS(status));
    assert(th);
    (void)status;

    assert(lock->owner == th);

    lock->owner = NULL;

    atomic_flag_clear_explicit(&lock->locked, memory_order_release);

    StThread_UnlockPreemption();
}

StStatus StSpinlock_LockAndSaveIrq(struct StSpinlock *lock __in, uint32_t *irqstate __out)
{
    assert(irqstate);

    StStatus status;
    StThread_InternalRef th;

    *irqstate = StA_SaveInterrupt();
    StA_DisableInterrupt();

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) {
        StA_RestoreInterrupt(*irqstate);
        return status;
    }

    while (atomic_flag_test_and_set_explicit(&lock->locked, memory_order_acquire)) {
        StA_Pause();
    }

    lock->owner = th;

    return STATUS_SUCCESS;
}

StStatus StSpinlock_TryLockAndSaveIrq(
    struct StSpinlock *lock __in, uint32_t *irqstate __out, int *locked __out
)
{
    assert(irqstate);
    assert(locked);

    StStatus status;
    StThread_InternalRef th;

    *irqstate = StA_SaveInterrupt();
    StA_DisableInterrupt();

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) {
        StA_RestoreInterrupt(*irqstate);
        return status;
    }

    if (atomic_flag_test_and_set_explicit(&lock->locked, memory_order_acquire)) {
        StA_RestoreInterrupt(*irqstate);
        *locked = 0;
        return STATUS_SUCCESS;
    }

    lock->owner = th;

    *locked = 1;
    return STATUS_SUCCESS;
}

void StSpinlock_UnlockAndRestoreIrq(struct StSpinlock *lock __in, uint32_t irqstate __in)
{
    StStatus status;
    StThread_InternalRef th;

    status = StScheduler_GetCurrentThread(&th);
    assert(CHECK_SUCCESS(status));
    assert(th);
    (void)status;

    assert(lock->owner == th);

    lock->owner = NULL;

    atomic_flag_clear_explicit(&lock->locked, memory_order_release);

    StA_RestoreInterrupt(irqstate);
}
