#include <uacpi/kernel_api.h>

#include <stddef.h>
#include <stdint.h>

#include <uacpi/platform/types.h>
#include <uacpi/types.h>

#include <strata/mm/pool.h>
#include <strata/plat/time.h>
#include <strata/raw_spinlock.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/thread_refs.h>

#define MODULE_NAME "acpi"

struct acpi_event {
    struct StRawSpinlock lock;
    uint32_t counter;
    StThread_InternalRef waiters;
};

static void wake_thread(StThread_InternalRef thread)
{
    thread->mutex_blocking_next = NULL;
    thread->sleep_until_uptime_ns = 0;
    thread->state = THREAD_STATE_RUNNING;
}

static int is_waiting(struct acpi_event *event, StThread_InternalRef thread)
{
    for (StThread_InternalRef current = event->waiters; current;
         current = current->mutex_blocking_next) {
        if (current == thread) return 1;
    }

    return 0;
}

static void add_waiter(struct acpi_event *event, StThread_InternalRef thread)
{
    if (is_waiting(event, thread)) return;

    thread->mutex_blocking_next = event->waiters;
    event->waiters = thread;
}

static void remove_waiter(struct acpi_event *event, StThread_InternalRef thread)
{
    StThread_InternalRef prev = NULL;

    for (StThread_InternalRef current = event->waiters; current;
         current = current->mutex_blocking_next) {
        if (current != thread) {
            prev = current;
            continue;
        }

        if (prev) {
            prev->mutex_blocking_next = current->mutex_blocking_next;
        } else {
            event->waiters = current->mutex_blocking_next;
        }
        current->mutex_blocking_next = NULL;
        return;
    }
}

uacpi_handle uacpi_kernel_create_event(void)
{
    StStatus status;
    struct acpi_event *event;

    status = StPool_AllocateClear(sizeof(*event), (void **)&event);
    if (!CHECK_SUCCESS(status)) return NULL;

    StRawSpinlock_Init(&event->lock);

    return event;
}

void uacpi_kernel_free_event(uacpi_handle event)
{
    if (!event) return;

    StPool_Free(event);
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle event, uacpi_u16 timeout_ms)
{
    struct acpi_event *acpi_event = event;
    StThread_InternalRef current_thread;
    uint64_t deadline_ns = 0;
    int finite_timeout = timeout_ms != UINT16_MAX;

    if (!acpi_event) return UACPI_FALSE;

    if (finite_timeout) {
        StTimeP_GetUptimeNanoseconds(&deadline_ns);
        deadline_ns += (uint64_t)timeout_ms * 1000000;
    }

    if (!timeout_ms) {
        uint32_t irqstate;
        uacpi_bool signaled = UACPI_FALSE;

        StRawSpinlock_LockAndSaveIrq(&acpi_event->lock, &irqstate);
        if (acpi_event->counter) {
            acpi_event->counter--;
            signaled = UACPI_TRUE;
        }
        StRawSpinlock_UnlockAndRestoreIrq(&acpi_event->lock, irqstate);

        return signaled;
    }

    if (!CHECK_SUCCESS(StScheduler_GetCurrentThread(&current_thread)) || !current_thread) {
        return UACPI_FALSE;
    }

    for (;;) {
        uint32_t irqstate;
        uint64_t now_ns;

        StTimeP_GetUptimeNanoseconds(&now_ns);
        StRawSpinlock_LockAndSaveIrq(&acpi_event->lock, &irqstate);

        if (acpi_event->counter) {
            acpi_event->counter--;
            remove_waiter(acpi_event, current_thread);
            wake_thread(current_thread);
            StRawSpinlock_UnlockAndRestoreIrq(&acpi_event->lock, irqstate);
            return UACPI_TRUE;
        }

        if (finite_timeout && now_ns >= deadline_ns) {
            remove_waiter(acpi_event, current_thread);
            wake_thread(current_thread);
            StRawSpinlock_UnlockAndRestoreIrq(&acpi_event->lock, irqstate);
            return UACPI_FALSE;
        }

        add_waiter(acpi_event, current_thread);
        if (finite_timeout) {
            current_thread->state = THREAD_STATE_SLEEPING;
            current_thread->sleep_until_uptime_ns = deadline_ns;
        } else {
            current_thread->state = THREAD_STATE_BLOCKING;
        }

        StRawSpinlock_UnlockAndRestoreIrq(&acpi_event->lock, irqstate);

        StThread_Yield();
    }
}

void uacpi_kernel_signal_event(uacpi_handle event)
{
    struct acpi_event *acpi_event = event;
    StThread_InternalRef waiter;
    uint32_t irqstate;

    if (!acpi_event) return;

    StRawSpinlock_LockAndSaveIrq(&acpi_event->lock, &irqstate);

    acpi_event->counter++;

    waiter = acpi_event->waiters;
    if (waiter) {
        acpi_event->waiters = waiter->mutex_blocking_next;
        wake_thread(waiter);
    }

    StRawSpinlock_UnlockAndRestoreIrq(&acpi_event->lock, irqstate);
}

void uacpi_kernel_reset_event(uacpi_handle event)
{
    struct acpi_event *acpi_event = event;
    uint32_t irqstate;

    if (!acpi_event) return;

    StRawSpinlock_LockAndSaveIrq(&acpi_event->lock, &irqstate);
    acpi_event->counter = 0;
    StRawSpinlock_UnlockAndRestoreIrq(&acpi_event->lock, irqstate);
}
