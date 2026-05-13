#include <uacpi/kernel_api.h>

#include <stdatomic.h>
#include <stdint.h>

#include <uacpi/platform/types.h>
#include <uacpi/status.h>
#include <uacpi/types.h>

#include <strata/log.h>
#include <strata/raw_spinlock.h>
#include <strata/status.h>
#include <strata/thread.h>

#define MODULE_NAME "acpi"

#define ACPI_WORK_QUEUE_CAPACITY 64

struct acpi_work_item {
    uacpi_work_type type;
    uacpi_work_handler handler;
    uacpi_handle ctx;
};

static struct StRawSpinlock work_lock = {.locked = ATOMIC_FLAG_INIT, .irq_state = 0};
static struct acpi_work_item work_queue[ACPI_WORK_QUEUE_CAPACITY];
static unsigned int work_queue_head;
static unsigned int work_queue_tail;
static unsigned int work_queue_count;
static unsigned int work_running_count;
static StThread_StrongRef work_thread;
static StThread_InternalRef sleeping_work_thread;

extern atomic_uint_fast32_t uacpi_kernel_interrupt_inflight_count;

static void wake_work_thread(void)
{
    if (!sleeping_work_thread) return;

    sleeping_work_thread->state = THREAD_STATE_RUNNING;
    sleeping_work_thread = NULL;
}

static void work_thread_main(StThread_BorrowedRef thread)
{
    (void)thread;

    for (;;) {
        uint32_t irqstate;
        struct acpi_work_item item;

        StRawSpinlock_LockAndSaveIrq(&work_lock, &irqstate);

        if (!work_queue_count) {
            sleeping_work_thread = (StThread_InternalRef)thread;
            thread->state = THREAD_STATE_BLOCKING;
            StRawSpinlock_UnlockAndRestoreIrq(&work_lock, irqstate);
            StThread_Yield();
            continue;
        }

        item = work_queue[work_queue_head];
        work_queue_head = (work_queue_head + 1) % ACPI_WORK_QUEUE_CAPACITY;
        work_queue_count--;
        work_running_count++;

        StRawSpinlock_UnlockAndRestoreIrq(&work_lock, irqstate);

        (void)item.type;
        item.handler(item.ctx);

        StRawSpinlock_LockAndSaveIrq(&work_lock, &irqstate);
        work_running_count--;
        StRawSpinlock_UnlockAndRestoreIrq(&work_lock, irqstate);
    }
}

uacpi_status uacpi_kernel_ensure_work_thread(void)
{
    StStatus status;
    StThread_StrongRef thread;

    if (work_thread) return UACPI_STATUS_OK;

    status = StThread_CreateKernel(work_thread_main, TCF_DEFAULT, &thread);
    if (!CHECK_SUCCESS(status)) {
        LOG_ERROR(LM_CAT_ACPI, "failed to create ACPI work thread: %08X\n", status);
        return UACPI_STATUS_INTERNAL_ERROR;
    }

    work_thread = thread;

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_schedule_work(
    uacpi_work_type type, uacpi_work_handler handler, uacpi_handle ctx
)
{
    uint32_t irqstate;
    uacpi_status status;

    if (!handler) return UACPI_STATUS_INVALID_ARGUMENT;

    status = uacpi_kernel_ensure_work_thread();
    if (status != UACPI_STATUS_OK) return status;

    StRawSpinlock_LockAndSaveIrq(&work_lock, &irqstate);

    if (work_queue_count >= ACPI_WORK_QUEUE_CAPACITY) {
        StRawSpinlock_UnlockAndRestoreIrq(&work_lock, irqstate);
        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    work_queue[work_queue_tail] = (struct acpi_work_item){
        .type = type,
        .handler = handler,
        .ctx = ctx,
    };
    work_queue_tail = (work_queue_tail + 1) % ACPI_WORK_QUEUE_CAPACITY;
    work_queue_count++;

    wake_work_thread();

    StRawSpinlock_UnlockAndRestoreIrq(&work_lock, irqstate);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void)
{
    for (;;) {
        if (!atomic_load(&uacpi_kernel_interrupt_inflight_count)) break;
        StThread_Sleep(1);
    }

    for (;;) {
        uint32_t irqstate;
        int done;

        StRawSpinlock_LockAndSaveIrq(&work_lock, &irqstate);
        done = !work_queue_count && !work_running_count;
        StRawSpinlock_UnlockAndRestoreIrq(&work_lock, irqstate);

        if (done) break;

        StThread_Sleep(1);
    }

    return UACPI_STATUS_OK;
}
