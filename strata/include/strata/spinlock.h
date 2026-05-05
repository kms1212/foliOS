#ifndef __STRATA_SPINLOCK_H__
#define __STRATA_SPINLOCK_H__

#include <stdatomic.h>

#include <strata/compiler.h>
#include <strata/status.h>
#include <strata/thread.h>

struct StSpinlock {
    struct StThread *owner;
    atomic_flag locked;
};

void StSpinlock_Init(struct StSpinlock *lock __in);

StStatus StSpinlock_Lock(struct StSpinlock *lock __in);
StStatus StSpinlock_TryLock(struct StSpinlock *lock __in, int *locked __out);
StStatus StSpinlock_Unlock(struct StSpinlock *lock __in);

StStatus StSpinlock_LockAndSaveIrq(struct StSpinlock *lock __in, uint32_t *irqstate __out);
StStatus StSpinlock_TryLockAndSaveIrq(
    struct StSpinlock *lock __in, uint32_t *irqstate __out, int *locked __out
);
StStatus StSpinlock_UnlockAndRestoreIrq(struct StSpinlock *lock __in, uint32_t irqstate __in);

#endif  // __STRATA_SPINLOCK_H__
