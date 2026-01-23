#ifndef __STRATA_SPINLOCK_H__
#define __STRATA_SPINLOCK_H__

#include <strata/thread.h>
#include <strata/status.h>

struct StSpinlock {
    struct StThread *owner;
    volatile int locked;
};

StStatus StSpinlock_Init(struct StSpinlock *lock);

StStatus StSpinlock_Lock(struct StSpinlock *lock);
StStatus StSpinlock_TryLock(struct StSpinlock *lock);
StStatus StSpinlock_Unlock(struct StSpinlock *lock);

StStatus StSpinlock_TryLockAndSaveIrq(struct StSpinlock *lock, uint32_t *irqstate);
StStatus StSpinlock_UnlockAndRestoreIrq(struct StSpinlock *lock, uint32_t irqstate);

#endif // __STRATA_SPINLOCK_H__
