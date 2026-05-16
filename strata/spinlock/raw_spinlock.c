#include <strata/raw_spinlock.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>

#include <strata/arch/interrupt.h>
#include <strata/arch/intrinsics/misc.h>

#include <strata/compiler.h>

void StRawSpinlock_Init(struct StRawSpinlock *lock __in)
{
    assert(lock);

    atomic_flag_clear(&lock->locked);
}

void StRawSpinlock_LockAndSaveIrq(struct StRawSpinlock *lock __in, uint32_t *irqstate __out)
{
    assert(lock);
    assert(irqstate);

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
    assert(lock);
    assert(irqstate);
    assert(locked);

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
    assert(lock);

    atomic_flag_clear_explicit(&lock->locked, memory_order_release);

    StA_RestoreInterrupt(irqstate);
}
