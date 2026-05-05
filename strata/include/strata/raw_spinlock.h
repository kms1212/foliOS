#ifndef __STRATA_RAW_SPINLOCK_H__
#define __STRATA_RAW_SPINLOCK_H__

#include <stdatomic.h>
#include <stdint.h>

#include <strata/status.h>
#include <strata/thread.h>

struct StRawSpinlock {
    atomic_flag locked;
    uint32_t irq_state;
};

void StRawSpinlock_Init(struct StRawSpinlock *lock __in);

void StRawSpinlock_LockAndSaveIrq(struct StRawSpinlock *lock __in, uint32_t *irqstate __out);
void StRawSpinlock_TryLockAndSaveIrq(
    struct StRawSpinlock *lock __in, uint32_t *irqstate __out, int *locked __out
);
void StRawSpinlock_UnlockAndRestoreIrq(struct StRawSpinlock *lock __in, uint32_t irqstate __in);

#endif  // __STRATA_RAW_SPINLOCK_H__
