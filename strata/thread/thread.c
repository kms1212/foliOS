#include <strata/thread.h>

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

#include <strata/plat/thread.h>
#include <strata/plat/time.h>

#include <strata/compiler.h>
#include <strata/log.h>
#include <strata/mm/pool.h>
#include <strata/mm/types.h>
#include <strata/panic.h>
#include <strata/process.h>
#include <strata/scheduler.h>
#include <strata/status.h>

#define MODULE_NAME "thread"

static atomic_int preemption_disable_depth = 0;

static void add_thread_to_process(struct StThread *thread)
{
    struct StProcess *process;

    if (!thread || !thread->process) return;

    process = thread->process;
    thread->process_next = NULL;

    if (!process->thread_list_head) {
        process->thread_list_head = process->thread_list_tail = thread;
    } else {
        process->thread_list_tail->process_next = thread;
        process->thread_list_tail = thread;
    }
}

static void remove_thread_from_process(struct StThread *thread)
{
    struct StProcess *process;
    struct StThread *prev;

    if (!thread || !thread->process) return;

    process = thread->process;

    prev = NULL;
    if (process->thread_list_head == thread) {
        process->thread_list_head = thread->process_next;
    } else {
        prev = process->thread_list_head;
        while (prev && prev->process_next != thread) {
            prev = prev->process_next;
        }
        if (prev) {
            prev->process_next = thread->process_next;
        }
    }

    if (process->thread_list_tail == thread) {
        process->thread_list_tail = prev;
    }

    thread->process_next = NULL;
}

StStatus StThread_Init(struct StThread **main_thread __out)
{
    StStatus status;
    struct StThread *main_th = NULL;
    int added_thread_to_scheduler = 0;

    status = StPool_AllocateClear(sizeof(*main_th), (void **)&main_th);
    if (!CHECK_SUCCESS(status)) goto has_error;

    main_th->id = 0;
    main_th->state = THREAD_STATE_RUNNING;
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
        StPool_Free(main_th);
    }

    return status;
}

void StThread_LockPreemption(void)
{
    int depth = atomic_fetch_add(&preemption_disable_depth, 1);
    if (depth < 0) {
        St_Panic(STATUS_CONFLICTING_STATE, "preemption enable underflow");
    }
}

void StThread_UnlockPreemption(void)
{
    int depth = atomic_fetch_sub(&preemption_disable_depth, 1);
    if (depth < 1) {
        St_Panic(STATUS_CONFLICTING_STATE, "preemption enable underflow");
    }
}

int StThread_IsPreemptionEnabled(void)
{
    return atomic_load(&preemption_disable_depth) == 0;
}

StStatus StThread_CreateKernel(
    StThread_EntryFunction entry __in,
    St_PageCount stack_page_count __in,
    struct StThread **threadout __out
)
{
    static StThread_Id new_thread_id = (StThread_Id)1;

    StStatus status;
    struct StThread *th = NULL;
    int stack_allocated = 0;
    int added_thread_to_scheduler = 0;

    StThread_LockPreemption();

    /* create thread object */
    status = StPool_AllocateClear(sizeof(*th), (void **)&th);
    if (!CHECK_SUCCESS(status)) goto has_error;

    th->id = new_thread_id++;
    th->state = THREAD_STATE_PENDING;
    th->type = THREAD_TYPE_KERNEL;

    /* prepare stack */
    th->kmode_stack_page_count = stack_page_count;
    th->kmode_entry = entry;

    status = StThreadP_AllocateThreadKernelStack(th);
    if (!CHECK_SUCCESS(status)) goto has_error;
    stack_allocated = 1;

    status = StThreadP_SetupThreadKernelStack(th);
    if (!CHECK_SUCCESS(status)) goto has_error;

    /* add thread object to list */
    status = StScheduler_AddThread(th);
    if (!CHECK_SUCCESS(status)) goto has_error;
    added_thread_to_scheduler = 1;

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "created kernel thread #%d\n", th->id);

    *threadout = th;

    StThread_UnlockPreemption();

    return STATUS_SUCCESS;

has_error:
    if (th && added_thread_to_scheduler) {
        StScheduler_RemoveThread(th);
    }

    if (th && stack_allocated) {
        StThreadP_FreeThreadKernelStack(th);
    }

    if (th) {
        StPool_Free(th);
    }

    StThread_UnlockPreemption();

    return status;
}

StStatus StThread_CreateUserMain(
    struct StProcess *process __in,
    uintptr_t entry __in,
    St_PageCount kstack_page_count __in,
    St_PageCount ustack_page_count __in,
    int arg_count __in,
    const char *const *args __in,
    int env_count __in,
    const char *const *envs __in,
    struct StThread **threadout __out
)
{
    static StThread_Id new_thread_id = (StThread_Id)16384;

    StStatus status;
    struct StThread *th = NULL;
    int added_thread_to_scheduler = 0;
    int kstack_allocated = 0;
    int ustack_allocated = 0;
    int added_thread_to_process = 0;

    StThread_LockPreemption();

    /* create thread object */
    status = StPool_AllocateClear(sizeof(*th), (void **)&th);
    if (!CHECK_SUCCESS(status)) goto has_error;

    th->id = new_thread_id++;
    th->state = THREAD_STATE_PENDING;
    th->type = THREAD_TYPE_USER;
    th->process = process;
    th->is_main = 1;

    th->umode_entry = entry;

    /* prepare user stack */
    th->umode_stack_page_count = ustack_page_count;

    status = StThreadP_AllocateThreadUserStack(th);
    if (!CHECK_SUCCESS(status)) goto has_error;
    ustack_allocated = 1;

    status = StThreadP_SetupThreadUserStack(th, arg_count, args, env_count, envs);
    if (!CHECK_SUCCESS(status)) goto has_error;

    /* prepare kernel stack */
    th->kmode_stack_page_count = kstack_page_count;

    status = StThreadP_AllocateThreadKernelStack(th);
    if (!CHECK_SUCCESS(status)) goto has_error;
    kstack_allocated = 1;

    status = StThreadP_SetupThreadKernelStack(th);
    if (!CHECK_SUCCESS(status)) goto has_error;

    /* add thread object to list */
    status = StScheduler_AddThread(th);
    if (!CHECK_SUCCESS(status)) goto has_error;
    added_thread_to_scheduler = 1;

    add_thread_to_process(th);
    added_thread_to_process = 1;

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "created user thread #%d\n", th->id);

    *threadout = th;

    StThread_UnlockPreemption();

    return STATUS_SUCCESS;

has_error:
    if (th && added_thread_to_process) {
        remove_thread_from_process(th);
    }

    if (th && added_thread_to_scheduler) {
        StScheduler_RemoveThread(th);
    }

    if (th && ustack_allocated) {
        StThreadP_FreeThreadUserStack(th);
    }

    if (th && kstack_allocated) {
        StThreadP_FreeThreadKernelStack(th);
    }

    if (th) {
        StPool_Free(th);
    }

    StThread_UnlockPreemption();

    return status;
}

StStatus StThread_Remove(struct StThread *th)
{
    if (th->type == THREAD_TYPE_MAIN) return STATUS_INVALID_THREAD;
    if (th->state != THREAD_STATE_FINISHED) return STATUS_THREAD_NOT_FINISHED;

    th->is_dying = 1;

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "removing thread #%d\n", th->id);

    StThread_LockPreemption();
    remove_thread_from_process(th);
    StScheduler_RemoveThread(th);
    StThread_UnlockPreemption();

    StThreadP_FreeThreadKernelStack(th);

    if (th->type == THREAD_TYPE_USER) {
        StThreadP_FreeThreadUserStack(th);

        if (th->is_main && !th->process->is_dying) {
            StProcess_Remove(th->process);
        }
    }

    StPool_Free(th);

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

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "detaching thread #%d\n", thread->id);

    return STATUS_SUCCESS;
}

StStatus StThread_Wait(struct StThread **list __in, int count __in, int timeout_ms __in)
{
    StStatus status;
    struct StThread *current_thread;

    status = StScheduler_GetCurrentThread(&current_thread);
    if (!CHECK_SUCCESS(status)) return status;

    for (int i = 0; i < count; i++) {
        if (list[i]->is_detached) return STATUS_CONFLICTING_STATE;
    }

    StThread_LockPreemption();

    // TODO: implement timeout

    current_thread->wait_list = list;
    current_thread->wait_count = count;
    current_thread->wait_timeout_ms = timeout_ms;
    current_thread->state = THREAD_STATE_WAITING;

    StThread_UnlockPreemption();

    StThread_Yield();

    return STATUS_SUCCESS;
}

StStatus StThread_Sleep(int timeout_ms __in)
{
    StStatus status;
    struct StThread *current_thread;

    status = StScheduler_GetCurrentThread(&current_thread);
    if (!CHECK_SUCCESS(status)) return status;

    StThread_LockPreemption();

    current_thread->state = THREAD_STATE_SLEEPING;
    current_thread->sleep_until_uptime_us =
        StTimeP_GetUptimeMicroseconds() + ((uint64_t)timeout_ms * 1000);

    StThread_UnlockPreemption();

    StThread_Yield();

    return STATUS_SUCCESS;
}

void StThread_Yield(void)
{
    StThreadP_Yield();
}

__noreturn void StThread_Exit(void)
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

    current_thread->state = THREAD_STATE_FINISHED;

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "thread #%d finished\n", current_thread->id);

    StThread_Yield();

    for (;;) {
    }
}
