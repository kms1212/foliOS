#ifndef __STRATA_MUTEX_H__
#define __STRATA_MUTEX_H__

#include <strata/status.h>
#include <strata/thread.h>

struct __capability("mutex") StMutex {
    volatile int locked;
    struct StThread *owner;
    struct StThread *blocking_threads;
};

StStatus StMutex_Init(struct StMutex *mtx);

StStatus StMutex_Lock(struct StMutex *mtx) __acquires(mtx);
StStatus StMutex_LockWithTimeout(struct StMutex *mtx, int timeout_ms);
StStatus StMutex_TryLock(struct StMutex *mtx, int *locked);
StStatus StMutex_Unlock(struct StMutex *mtx) __releases(mtx);

#endif  // __STRATA_MUTEX_H__
