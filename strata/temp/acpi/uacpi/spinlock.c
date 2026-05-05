#include <uacpi/kernel_api.h>

#include <stddef.h>

#include <uacpi/platform/types.h>
#include <uacpi/status.h>

#include <strata/mm/pool.h>
#include <strata/spinlock.h>
#include <strata/status.h>

#define MODULE_NAME "acpi"

uacpi_handle uacpi_kernel_create_spinlock(void)
{
    StStatus status;
    struct StSpinlock *spinlock;

    status = StPool_Allocate(sizeof(*spinlock), (void **)&spinlock);
    if (!CHECK_SUCCESS(status)) return NULL;

    StSpinlock_Init(spinlock);

    return spinlock;
}

void uacpi_kernel_free_spinlock(uacpi_handle spinlock)
{
    StPool_Free(spinlock);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle spinlock)
{
    StStatus status;
    uint32_t irqstate;
    uacpi_cpu_flags uacpi_irqstate;

    status = StSpinlock_LockAndSaveIrq(spinlock, &irqstate);
    if (!CHECK_SUCCESS(status)) return 0;

    uacpi_irqstate = irqstate;

    return uacpi_irqstate;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle spinlock, uacpi_cpu_flags flags)
{
    uint32_t irqstate = flags;

    StSpinlock_UnlockAndRestoreIrq(spinlock, irqstate);
}
