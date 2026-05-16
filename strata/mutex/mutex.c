#include <strata/mutex.h>

#include <assert.h>
#include <stdint.h>

#include <strata/plat/time.h>

#include <strata/compiler.h>
#include <strata/log.h>
#include <strata/raw_spinlock.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/thread_refs.h>

#define MODULE_NAME "mutex"

void StMutex_Init(struct StMutex *mtx __in)
{
    assert(mtx);

    StRawSpinlock_Init(&mtx->state_lock);
    mtx->locked = 0;
    mtx->owner = NULL;
    mtx->blocking_threads = NULL;
}

static void add_blocking_thread(struct StMutex *mtx, StThread_InternalRef th)
{
    StThread_InternalRef last_blocking_th;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "blocking thread #%d\n", th->id);

    if (mtx->blocking_threads == th) return;

    if (!mtx->blocking_threads) {
        mtx->blocking_threads = th;
        th->mutex_blocking_next = NULL;
        return;
    }

    last_blocking_th = mtx->blocking_threads;
    while (last_blocking_th) {
        if (last_blocking_th == th) return;
        if (!last_blocking_th->mutex_blocking_next) break;
        last_blocking_th = last_blocking_th->mutex_blocking_next;
    }

    last_blocking_th->mutex_blocking_next = th;
    th->mutex_blocking_next = NULL;
}

static void unblock_blocking_thread(struct StMutex *mtx)
{
    StThread_InternalRef th_to_unblock;

    if (!mtx->blocking_threads) return;

    th_to_unblock = mtx->blocking_threads;
    mtx->blocking_threads = th_to_unblock->mutex_blocking_next;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "unblocking thread #%d\n", th_to_unblock->id);

    th_to_unblock->mutex_blocking_next = NULL;
    th_to_unblock->sleep_until_uptime_ns = 0;
    if (th_to_unblock->state == THREAD_STATE_BLOCKING ||
        th_to_unblock->state == THREAD_STATE_SLEEPING) {
        th_to_unblock->state = THREAD_STATE_RUNNING;
    }
}

static void remove_blocking_thread(struct StMutex *mtx, StThread_InternalRef th)
{
    StThread_InternalRef current;
    StThread_InternalRef prev;

    if (!mtx || !th) return;

    prev = NULL;
    current = mtx->blocking_threads;
    while (current) {
        if (current == th) {
            if (prev) {
                prev->mutex_blocking_next = current->mutex_blocking_next;
            } else {
                mtx->blocking_threads = current->mutex_blocking_next;
            }

            current->mutex_blocking_next = NULL;
            return;
        }

        prev = current;
        current = current->mutex_blocking_next;
    }
}

StStatus StMutex_Lock(struct StMutex *mtx __in)
{
    assert(mtx);

    StStatus status;
    StThread_InternalRef th;

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) return status;

    for (;;) {
        uint32_t irqstate;

        StThread_LockPreemption();
        StRawSpinlock_LockAndSaveIrq(&mtx->state_lock, &irqstate);

        if (!mtx->locked) {
            remove_blocking_thread(mtx, th);
            mtx->locked = 1;
            mtx->owner = th;

            StRawSpinlock_UnlockAndRestoreIrq(&mtx->state_lock, irqstate);
            StThread_UnlockPreemption();
            return STATUS_SUCCESS;
        }

        th->state = THREAD_STATE_BLOCKING;
        add_blocking_thread(mtx, th);

        StRawSpinlock_UnlockAndRestoreIrq(&mtx->state_lock, irqstate);
        StThread_UnlockPreemption();
        StThread_Yield();
    }
}

StStatus StMutex_LockWithTimeout(struct StMutex *mtx __in, int timeout_ms __in)
{
    assert(mtx);

    StStatus status;
    StThread_InternalRef th;
    uint64_t deadline_ns;

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) return status;

    if (timeout_ms < 0) {
        return StMutex_Lock(mtx);
    }

    if (timeout_ms == 0) {
        int locked = 0;

        status = StMutex_TryLock(mtx, &locked);
        if (!CHECK_SUCCESS(status)) return status;

        return locked ? STATUS_SUCCESS : STATUS_TIMER_EXPIRED;
    }

    StTimeP_GetUptimeNanoseconds(&deadline_ns);
    deadline_ns += (uint64_t)timeout_ms * 1000000;

    for (;;) {
        uint64_t now_ns;
        uint32_t irqstate;

        StTimeP_GetUptimeNanoseconds(&now_ns);

        StThread_LockPreemption();
        StRawSpinlock_LockAndSaveIrq(&mtx->state_lock, &irqstate);

        if (!mtx->locked) {
            remove_blocking_thread(mtx, th);
            mtx->locked = 1;
            mtx->owner = th;
            th->mutex_blocking_next = NULL;
            th->sleep_until_uptime_ns = 0;

            StRawSpinlock_UnlockAndRestoreIrq(&mtx->state_lock, irqstate);
            StThread_UnlockPreemption();
            return STATUS_SUCCESS;
        }

        if (now_ns >= deadline_ns) {
            remove_blocking_thread(mtx, th);
            th->mutex_blocking_next = NULL;
            th->sleep_until_uptime_ns = 0;
            th->state = THREAD_STATE_RUNNING;

            StRawSpinlock_UnlockAndRestoreIrq(&mtx->state_lock, irqstate);
            StThread_UnlockPreemption();
            return STATUS_TIMER_EXPIRED;
        }

        th->sleep_until_uptime_ns = deadline_ns;
        th->state = THREAD_STATE_SLEEPING;
        add_blocking_thread(mtx, th);

        StRawSpinlock_UnlockAndRestoreIrq(&mtx->state_lock, irqstate);
        StThread_UnlockPreemption();
        StThread_Yield();
    }
}

StStatus StMutex_TryLock(struct StMutex *mtx __in, int *locked __out_optional)
{
    assert(mtx);

    StStatus status;
    StThread_InternalRef th;
    uint32_t irqstate;

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) return status;

    StThread_LockPreemption();
    StRawSpinlock_LockAndSaveIrq(&mtx->state_lock, &irqstate);

    if (mtx->locked) {
        StRawSpinlock_UnlockAndRestoreIrq(&mtx->state_lock, irqstate);
        StThread_UnlockPreemption();

        if (locked) *locked = 0;

        return STATUS_SUCCESS;
    }

    mtx->locked = 1;
    mtx->owner = th;

    StRawSpinlock_UnlockAndRestoreIrq(&mtx->state_lock, irqstate);
    StThread_UnlockPreemption();

    if (locked) *locked = 1;

    return STATUS_SUCCESS;
}

void StMutex_Unlock(struct StMutex *mtx __in)
{
    assert(mtx);

    StStatus status;
    StThread_InternalRef th;
    uint32_t irqstate;

    status = StScheduler_GetCurrentThread(&th);
    assert(CHECK_SUCCESS(status));
    assert(th);
    (void)status;

    StThread_LockPreemption();
    StRawSpinlock_LockAndSaveIrq(&mtx->state_lock, &irqstate);

    assert(mtx->owner == th);

    mtx->locked = 0;
    mtx->owner = NULL;

    unblock_blocking_thread(mtx);

    StRawSpinlock_UnlockAndRestoreIrq(&mtx->state_lock, irqstate);
    StThread_UnlockPreemption();
}
