#include <strata/spinlock.h>

#include <stdatomic.h>
#include <stdint.h>

#include <strata/arch/interrupt.h>
#include <strata/arch/intrinsics/misc.h>

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
    struct StThread *th;

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
    StStatus status;
    struct StThread *th;

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) return status;

    StThread_LockPreemption();

    if (atomic_flag_test_and_set_explicit(&lock->locked, memory_order_acquire)) {
        StThread_UnlockPreemption();

        if (locked) *locked = 0;
        return STATUS_SUCCESS;
    }

    lock->owner = th;

    if (locked) *locked = 1;
    return STATUS_SUCCESS;
}

StStatus StSpinlock_Unlock(struct StSpinlock *lock __in)
{
    StStatus status;
    struct StThread *th;

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) return status;

    if (lock->owner != th) {
        return STATUS_INVALID_THREAD;
    }

    lock->owner = NULL;

    atomic_flag_clear_explicit(&lock->locked, memory_order_release);

    StThread_UnlockPreemption();

    return STATUS_SUCCESS;
}

StStatus StSpinlock_LockAndSaveIrq(struct StSpinlock *lock __in, uint32_t *irqstate __out)
{
    StStatus status;
    struct StThread *th;

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
    StStatus status;
    struct StThread *th;

    *irqstate = StA_SaveInterrupt();
    StA_DisableInterrupt();

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) {
        StA_RestoreInterrupt(*irqstate);
        return status;
    }

    if (atomic_flag_test_and_set_explicit(&lock->locked, memory_order_acquire)) {
        StA_RestoreInterrupt(*irqstate);
        if (locked) *locked = 0;
        return STATUS_SUCCESS;
    }

    lock->owner = th;

    if (locked) *locked = 1;
    return STATUS_SUCCESS;
}

StStatus StSpinlock_UnlockAndRestoreIrq(struct StSpinlock *lock __in, uint32_t irqstate __in)
{
    StStatus status;
    struct StThread *th;

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) return status;

    if (lock->owner != th) {
        return STATUS_INVALID_THREAD;
    }

    lock->owner = NULL;

    atomic_flag_clear_explicit(&lock->locked, memory_order_release);

    StA_RestoreInterrupt(irqstate);

    return STATUS_SUCCESS;
}
