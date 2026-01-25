#ifndef __STRATA_MUTEX_H__
#define __STRATA_MUTEX_H__

#include <strata/status.h>
#include <strata/thread.h>

struct StMutex {
    volatile int locked;
    struct StThread *owner;
    struct StThread *blocking_threads;
};

StStatus StMutex_Init(struct StMutex *mtx);

StStatus StMutex_Lock(struct StMutex *mtx);
StStatus StMutex_LockWithTimeout(struct StMutex *mtx, int timeout_ms);
StStatus StMutex_TryLock(struct StMutex *mtx);
StStatus StMutex_Unlock(struct StMutex *mtx);

#endif  // __STRATA_MUTEX_H__
