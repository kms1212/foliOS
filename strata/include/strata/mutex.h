#ifndef __STRATA_MUTEX_H__
#define __STRATA_MUTEX_H__

#include <strata/raw_spinlock.h>
#include <strata/status.h>
#include <strata/thread_refs.h>

struct __capability("mutex") StMutex {
    struct StRawSpinlock state_lock;
    int locked;
    StThread_InternalRef owner;
    StThread_InternalRef blocking_threads;
};

void StMutex_Init(struct StMutex *mtx __in);

StStatus StMutex_Lock(struct StMutex *mtx __in) __acquires(mtx);
StStatus StMutex_LockWithTimeout(struct StMutex *mtx __in, int timeout_ms __in);
StStatus StMutex_TryLock(struct StMutex *mtx __in, int *locked __out_optional);
void StMutex_Unlock(struct StMutex *mtx __in) __releases(mtx);

#endif  // __STRATA_MUTEX_H__
