#include <strata/thread.h>

#include <stdlib.h>

#include <strata/arch/mmu.h>

#include <strata/plat/time.h>

#include <strata/panic.h>
#include <strata/log.h>
#include <strata/scheduler.h>
#include <strata/macros.h>

#define MODULE_NAME "thread"

static volatile int preemption_enabled = 0;

StStatus StThread_Init(struct StThread **main_thread __out)
{
    StStatus status;
    struct StThread *main_th = NULL;
    int added_thread_to_scheduler = 0;

    main_th = calloc(1, sizeof(*main_th));
    if (!main_th) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    main_th->id = 0;
    main_th->status = THREAD_STATE_RUNNING;
    main_th->type = THREAD_TYPE_MAIN;

    status = StScheduler_AddThread(main_th);
    if (!CHECK_SUCCESS(status)) goto has_error;
    added_thread_to_scheduler = 1;

    status = StScheduler_SetCurrentThread(main_th);
    if (!CHECK_SUCCESS(status)) goto has_error;

    *main_thread = main_th;
    
    return STATUS_SUCCESS;

has_error:
    if (added_thread_to_scheduler) {
        StScheduler_RemoveThread(main_th);
    }

    if (main_th) {
        free(main_th);
    }

    return status;
}

void StThread_EnablePreemption(void)
{
    preemption_enabled = 1;
}

void StThread_DisablePreemption(void)
{
    preemption_enabled = 0;
}

int StThread_IsPreemptionEnabled(void)
{
    return preemption_enabled;
}

StStatus StThread_CreateKernel(
    StThread_EntryFunction entry __in,
    size_t stack_size __in,
    struct StThread **threadout __out
)
{
    static StThread_Id new_thread_id = (StThread_Id)1;

    StStatus status;
    int prev_preemption_enabled = preemption_enabled;
    struct StThread *th = NULL;
    int stack_allocated = 0;
    int added_thread_to_scheduler = 0;

    StThread_DisablePreemption();

    /* create thread object */
    th = calloc(1, sizeof(*th));
    if (!th) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    th->id = new_thread_id++;
    th->status = THREAD_STATE_PENDING;
    th->type = THREAD_TYPE_KERNEL;

    /* prepare stack */
    th->kmode_stack_page_count = ALIGN_DIV(stack_size, PAGE_SIZE);
    th->kmode_entry = entry;

    status = StThreadP_AllocateKThreadStack(th);
    if (!CHECK_SUCCESS(status)) goto has_error;
    stack_allocated = 1;

    status = StThreadP_SetupKThreadStack(th);
    if (!CHECK_SUCCESS(status)) goto has_error;

    /* add thread object to list */
    status = StScheduler_AddThread(th);
    if (!CHECK_SUCCESS(status)) goto has_error;
    added_thread_to_scheduler = 1;

    LOG_DEBUG("created kernel thread #%d\n", th->id);

    *threadout = th;

    if (prev_preemption_enabled) {
        StThread_EnablePreemption();
    }

    return STATUS_SUCCESS;

has_error:
    if (th && added_thread_to_scheduler) {
        StScheduler_RemoveThread(th);
    }

    if (th && stack_allocated) {
        StThreadP_FreeKThreadStack(th);
    }

    if (th) {
        free(th);
    }

    if (prev_preemption_enabled) {
        StThread_EnablePreemption();
    }

    return status;
}

StStatus StThread_CreateUser(
    struct StProcess *process __in,
    uintptr_t entry __in,
    size_t stack_size __in,
    uintptr_t ustack_top __in,
    struct StThread **threadout __out
)
{
    static StThread_Id new_thread_id = (StThread_Id)16384;

    StStatus status;
    int prev_preemption_enabled = preemption_enabled;
    struct StThread *th = NULL;
    int added_thread_to_scheduler = 0;
    int stack_allocated = 0;

    StThread_DisablePreemption();

    /* create thread object */
    th = calloc(1, sizeof(*th));
    if (!th) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    th->id = new_thread_id++;
    th->status = THREAD_STATE_PENDING;
    th->type = THREAD_TYPE_USER;
    th->owner = process;
    
    th->umode_entry = entry;
    th->umode_stack_ptr = ustack_top;

    /* prepare stack */
    th->kmode_stack_page_count = ALIGN_DIV(stack_size, PAGE_SIZE);

    status = StThreadP_AllocateKThreadStack(th);
    if (!CHECK_SUCCESS(status)) goto has_error;
    stack_allocated = 1;

    status = StThreadP_SetupKThreadStack(th);
    if (!CHECK_SUCCESS(status)) goto has_error;

    /* add thread object to list */
    status = StScheduler_AddThread(th);
    if (!CHECK_SUCCESS(status)) goto has_error;
    added_thread_to_scheduler = 1;

    LOG_DEBUG("created user thread #%d\n", th->id);

    *threadout = th;

    if (prev_preemption_enabled) {
        StThread_EnablePreemption();
    }

    return STATUS_SUCCESS;

has_error:
    if (th && added_thread_to_scheduler) {
        StScheduler_RemoveThread(th);
    }

    if (th && stack_allocated) {
        StThreadP_FreeKThreadStack(th);
    }

    if (th) {
        free(th);
    }

    if (prev_preemption_enabled) {
        StThread_EnablePreemption();
    }

    return status;
}

StStatus StThread_Remove(struct StThread *th)
{
    if (th->type == THREAD_TYPE_MAIN) return STATUS_INVALID_THREAD;
    if (th->status != THREAD_STATE_FINISHED) return STATUS_THREAD_NOT_FINISHED;

    LOG_DEBUG("removing thread #%d\n", th->id);

    StScheduler_RemoveThread(th);

    StThreadP_FreeKThreadStack(th);

    free(th);

    return STATUS_SUCCESS;
}

StStatus StThread_Detach(struct StThread *thread)
{
    StStatus status;
    struct StThread *current_thread;
    
    status = StScheduler_GetCurrentThread(&current_thread);
    if (!CHECK_SUCCESS(status)) return status;

    if (thread->wait_list) return STATUS_CONFLICTING_STATE;

    thread->is_detached = 1;

    LOG_DEBUG("detaching thread #%d\n", thread->id);

    return STATUS_SUCCESS;
}

StStatus StThread_Wait(
    struct StThread **list __in,
    int count __in,
    int timeout_ms __in
)
{
    StStatus status;
    struct StThread *current_thread;
    
    status = StScheduler_GetCurrentThread(&current_thread);
    if (!CHECK_SUCCESS(status)) return status;

    for (int i = 0; i < count; i++) {
        if (list[i]->is_detached) return STATUS_CONFLICTING_STATE;
    }

    StThread_DisablePreemption();

    // TODO: implement timeout

    current_thread->wait_list = list;
    current_thread->wait_count = count;
    current_thread->wait_timeout_ms = timeout_ms;
    current_thread->status = THREAD_STATE_WAITING;

    StThread_EnablePreemption();

    StThread_Yield();

    return STATUS_SUCCESS;
}

StStatus StThread_Sleep(int timeout_ms __in)
{
    StStatus status;
    struct StThread *current_thread;
    
    status = StScheduler_GetCurrentThread(&current_thread);
    if (!CHECK_SUCCESS(status)) return status;

    StThread_DisablePreemption();

    current_thread->status = THREAD_STATE_SLEEPING;
    current_thread->sleep_until_tick = StTimeP_GetGlobalTick() + timeout_ms * StTimeP_GetGlobalTickFrequency() / 1000;

    StThread_EnablePreemption();

    StThread_Yield();

    return STATUS_SUCCESS;
}

void StThread_Yield(void)
{
    StThreadP_Yield();
}

__noreturn
void StThread_Exit(void)
{
    StStatus status;
    struct StThread *current_thread;
    
    status = StScheduler_GetCurrentThread(&current_thread);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "cannot get current thread");
    }

    if (current_thread->type == THREAD_TYPE_MAIN) {
        St_Panic(STATUS_INVALID_THREAD, "cannot exit from main thread");
    }

    current_thread->status = THREAD_STATE_FINISHED;

    LOG_DEBUG("thread #%d finished\n", current_thread->id);

    StThread_Yield();

    for (;;) {}
}
