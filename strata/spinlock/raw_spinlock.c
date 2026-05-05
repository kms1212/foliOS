#include <strata/raw_spinlock.h>

#include <stdatomic.h>
#include <stdint.h>

#include <strata/arch/interrupt.h>
#include <strata/arch/intrinsics/misc.h>

#include <strata/status.h>

void StRawSpinlock_Init(struct StRawSpinlock *lock __in)
{
    atomic_flag_clear(&lock->locked);
}

void StRawSpinlock_LockAndSaveIrq(struct StRawSpinlock *lock __in, uint32_t *irqstate __out)
{
    *irqstate = StA_SaveInterrupt();
    StA_DisableInterrupt();

    while (atomic_flag_test_and_set_explicit(&lock->locked, memory_order_acquire)) {
        StA_Pause();
    }
}

void StRawSpinlock_TryLockAndSaveIrq(
    struct StRawSpinlock *lock __in, uint32_t *irqstate __out, int *locked __out
)
{
    *irqstate = StA_SaveInterrupt();
    StA_DisableInterrupt();

    if (atomic_flag_test_and_set_explicit(&lock->locked, memory_order_acquire)) {
        StA_RestoreInterrupt(*irqstate);
        *locked = 0;
        return;
    }

    *locked = 1;
}

void StRawSpinlock_UnlockAndRestoreIrq(struct StRawSpinlock *lock __in, uint32_t irqstate __in)
{
    atomic_flag_clear_explicit(&lock->locked, memory_order_release);

    StA_RestoreInterrupt(irqstate);
}
