#include <strata/mutex.h>

#include <strata/log.h>
#include <strata/scheduler.h>
#include <strata/thread.h>

#define MODULE_NAME "mutex"

StStatus StMutex_Init(struct StMutex *mtx)
{
    mtx->locked = 0;
    mtx->owner = NULL;

    return STATUS_SUCCESS;
}

static void add_blocking_thread(struct StMutex *mtx, struct StThread *th)
{
    struct StThread *last_blocking_th;

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
    struct StThread *th_to_unblock;

    if (!mtx->blocking_threads) return;

    th_to_unblock = mtx->blocking_threads;
    mtx->blocking_threads = th_to_unblock->mutex_blocking_next;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "unblocking thread #%d\n", th_to_unblock->id);

    th_to_unblock->mutex_blocking_next = NULL;
    if (th_to_unblock->state == THREAD_STATE_BLOCKING) {
        th_to_unblock->state = THREAD_STATE_RUNNING;
    }
}

StStatus StMutex_Lock(struct StMutex *mtx)
{
    StStatus status;
    struct StThread *th;

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) return status;

    StThread_LockPreemption();

    while (mtx->locked) {
        StThread_UnlockPreemption();

        th->state = THREAD_STATE_BLOCKING;

        add_blocking_thread(mtx, th);

        StThread_Yield();

        StThread_LockPreemption();
    }

    mtx->locked = 1;
    mtx->owner = th;

    StThread_UnlockPreemption();

    return STATUS_SUCCESS;
}

StStatus StMutex_TryLock(struct StMutex *mtx, int *locked)
{
    StStatus status;
    struct StThread *th;

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) return status;

    StThread_LockPreemption();

    if (mtx->locked) {
        StThread_UnlockPreemption();

        if (locked) *locked = 0;

        return STATUS_SUCCESS;
    }

    mtx->locked = 1;
    mtx->owner = th;

    StThread_UnlockPreemption();

    if (locked) *locked = 1;

    return STATUS_SUCCESS;
}

StStatus StMutex_Unlock(struct StMutex *mtx)
{
    StStatus status;
    struct StThread *th;

    status = StScheduler_GetCurrentThread(&th);
    if (!CHECK_SUCCESS(status)) return status;

    StThread_LockPreemption();

    if (mtx->owner != th) {
        StThread_UnlockPreemption();

        return STATUS_INVALID_THREAD;
    }

    mtx->locked = 0;
    mtx->owner = NULL;

    unblock_blocking_thread(mtx);

    StThread_UnlockPreemption();

    return STATUS_SUCCESS;
}
