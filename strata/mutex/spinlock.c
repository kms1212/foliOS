#include <strata/spinlock.h>

#include <stdint.h>

#include <strata/arch/interrupt.h>
#include <strata/arch/intrinsics/misc.h>

#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>

extern int _pc_irq_level;

StStatus StSpinlock_Init(struct StSpinlock *lock)
{
    lock->locked = 0;
    lock->owner = NULL;

    return STATUS_SUCCESS;
}

StStatus StSpinlock_Lock(struct StSpinlock *lock)
{
    StStatus status;
    struct StThread *th;

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) return status;

    StThread_LockPreemption();

    while (lock->locked) {
        /* busy wait */
        StA_Pause();
    }

    lock->locked = 1;
    lock->owner = th;

    StThread_UnlockPreemption();

    return STATUS_SUCCESS;
}

StStatus StSpinlock_TryLock(struct StSpinlock *lock, int *locked)
{
    StStatus status;
    struct StThread *th;

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) return status;

    StThread_LockPreemption();

    if (lock->locked) {
        StThread_UnlockPreemption();

        if (locked) *locked = 0;
        return STATUS_SUCCESS;
    }

    lock->locked = 1;
    lock->owner = th;

    StThread_UnlockPreemption();

    if (locked) *locked = 1;
    return STATUS_SUCCESS;
}

StStatus StSpinlock_Unlock(struct StSpinlock *lock)
{
    StStatus status;
    struct StThread *th;

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) return status;

    StThread_LockPreemption();

    if (lock->owner != th) {
        StThread_UnlockPreemption();
        return STATUS_INVALID_THREAD;
    }

    lock->locked = 0;
    lock->owner = NULL;

    StThread_UnlockPreemption();

    return STATUS_SUCCESS;
}

StStatus StSpinlock_TryLockAndSaveIrq(struct StSpinlock *lock, uint32_t *irqstate, int *locked)
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

    if (lock->locked) {
        StA_RestoreInterrupt(*irqstate);

        if (locked) *locked = 0;
        return STATUS_SUCCESS;
    }

    lock->locked = 1;
    lock->owner = th;

    if (locked) *locked = 1;
    return STATUS_SUCCESS;
}

StStatus StSpinlock_UnlockAndRestoreIrq(struct StSpinlock *lock, uint32_t irqstate)
{
    StStatus status;
    struct StThread *th;

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) return status;

    if (lock->owner != th) {
        StA_RestoreInterrupt(irqstate);
        return STATUS_INVALID_THREAD;
    }

    lock->locked = 0;
    lock->owner = NULL;

    StA_RestoreInterrupt(irqstate);

    return STATUS_SUCCESS;
}
