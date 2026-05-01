#include <strata/thread.h>

#include "config.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/mm.h>
#include <strata/plat/thread.h>
#include <strata/plat/time.h>

#include <strata/compiler.h>
#include <strata/log.h>
#include <strata/mm/pmm.h>
#include <strata/mm/pool.h>
#include <strata/mm/types.h>
#include <strata/panic.h>
#include <strata/process.h>
#include <strata/scheduler.h>
#include <strata/status.h>

#define MODULE_NAME                                   "thread"
#define THREAD_DEFERRED_REAP_MAX_PENDING_PAGES        ((St_PageCount)512)
#define THREAD_DEFERRED_REAP_LOW_FREE_FRAME_WATERMARK ((St_PageCount)4096)

static atomic_uint_fast32_t preemption_disable_depth_early = 0;
static atomic_uint_fast32_t thread_count = 0;
static struct StThread *deferred_thread_reap_head = NULL;
static struct StThread *deferred_thread_reap_tail = NULL;
static struct StProcess *deferred_process_reap_head = NULL;
static struct StProcess *deferred_process_reap_tail = NULL;
static St_PageCount deferred_reap_pending_pages = 0;

struct StThreadAllocationHeader {
    void *raw_ptr;
};

static StStatus allocate_thread_object(struct StThread **threadout)
{
    enum { THREAD_OBJECT_ALIGNMENT = 64 };

    StStatus status;
    void *raw_ptr = NULL;
    uintptr_t aligned_addr;
    struct StThreadAllocationHeader *header;
    size_t alloc_size;

    alloc_size =
        sizeof(struct StThreadAllocationHeader) + sizeof(struct StThread) + THREAD_OBJECT_ALIGNMENT - 1;

    status = StPool_AllocateClear(alloc_size, &raw_ptr);
    if (!CHECK_SUCCESS(status)) return status;

    aligned_addr = (uintptr_t)raw_ptr + sizeof(struct StThreadAllocationHeader);
    aligned_addr = (aligned_addr + THREAD_OBJECT_ALIGNMENT - 1) & ~(uintptr_t)(THREAD_OBJECT_ALIGNMENT - 1);

    header = (struct StThreadAllocationHeader *)(aligned_addr - sizeof(*header));
    header->raw_ptr = raw_ptr;

    if (threadout) {
        *threadout = (struct StThread *)aligned_addr;
    }

    return STATUS_SUCCESS;
}

static void free_thread_object(struct StThread *thread)
{
    struct StThreadAllocationHeader *header;

    if (!thread) return;

    header = (struct StThreadAllocationHeader *)((uintptr_t)thread - sizeof(*header));
    StPool_Free(header->raw_ptr);
}

static inline atomic_uint_fast32_t *get_preemption_disable_depth_ptr(void)
{
    struct StCpuLocalP_Data *cpu_local = StCpuLocalP_GetData();
    if (cpu_local) return &cpu_local->preemption_disable_depth;
    return &preemption_disable_depth_early;
}

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

static St_PageCount get_thread_reap_page_count(const struct StThread *thread)
{
    St_PageCount pages = 0;

    if (!thread) return 0;

    pages += thread->kmode_stack_page_count;
    if (thread->type == THREAD_TYPE_USER) {
        pages += thread->umode_stack_page_count;
    }

    return pages;
}

static St_PageCount get_process_reap_page_count(const struct StProcess *process)
{
    St_PageCount pages = 1;

    if (!process) return 0;

    pages += process->alloc_owner.page_usage_count;
    if (process->main_thread) {
        pages += get_thread_reap_page_count(process->main_thread);
    }

    return pages;
}

static int deferred_reap_needs_pressure_relief(St_PageCount reserved_pages)
{
    StStatus status;
    St_PageCount free_frames = 0;

    if (deferred_reap_pending_pages + reserved_pages > THREAD_DEFERRED_REAP_MAX_PENDING_PAGES) {
        return 1;
    }

    status = StPmm_GetFreeFrameCount(&free_frames);
    if (!CHECK_SUCCESS(status)) {
        return deferred_reap_pending_pages > THREAD_DEFERRED_REAP_MAX_PENDING_PAGES;
    }

    if (free_frames <= THREAD_DEFERRED_REAP_LOW_FREE_FRAME_WATERMARK) {
        return 1;
    }

    if (reserved_pages &&
        free_frames <= THREAD_DEFERRED_REAP_LOW_FREE_FRAME_WATERMARK + reserved_pages) {
        return 1;
    }

    return 0;
}

static void finalize_thread_storage(struct StThread *thread)
{
    if (!thread) return;

    StThreadP_FreeThreadKernelStack(thread);

    if (thread->type == THREAD_TYPE_USER) {
        StThreadP_FreeThreadUserStack(thread);
    }

    free_thread_object(thread);
}

static void enqueue_thread_for_reap(struct StThread *thread)
{
    if (!thread || thread->is_reap_queued) return;

    thread->next = NULL;
    thread->is_reap_queued = 1;
    thread->deferred_reap_page_count = get_thread_reap_page_count(thread);

    StThread_LockPreemption();

    if (!deferred_thread_reap_head) {
        deferred_thread_reap_head = deferred_thread_reap_tail = thread;
    } else {
        deferred_thread_reap_tail->next = thread;
        deferred_thread_reap_tail = thread;
    }

    deferred_reap_pending_pages += thread->deferred_reap_page_count;

    StThread_UnlockPreemption();
}

static void enqueue_process_for_reap(struct StProcess *process)
{
    if (!process || process->is_reap_queued) return;

    process->next = NULL;
    process->is_reap_queued = 1;
    process->deferred_reap_page_count = get_process_reap_page_count(process);
    if (process->main_thread) {
        process->main_thread->is_reap_queued = 1;
    }

    StThread_LockPreemption();

    if (!deferred_process_reap_head) {
        deferred_process_reap_head = deferred_process_reap_tail = process;
    } else {
        deferred_process_reap_tail->next = process;
        deferred_process_reap_tail = process;
    }

    deferred_reap_pending_pages += process->deferred_reap_page_count;

    StThread_UnlockPreemption();
}

static St_PageCount reap_one_deferred_item(void)
{
    St_PageCount reclaimed_pages = 0;
    struct StProcess *process = NULL;
    struct StThread *thread = NULL;

    StThread_LockPreemption();

    if (deferred_process_reap_head) {
        process = deferred_process_reap_head;
        deferred_process_reap_head = process->next;
        if (!deferred_process_reap_head) {
            deferred_process_reap_tail = NULL;
        }

        reclaimed_pages = process->deferred_reap_page_count;
        process->next = NULL;
        process->is_reap_queued = 0;
        process->deferred_reap_page_count = 0;
    } else if (deferred_thread_reap_head) {
        thread = deferred_thread_reap_head;
        deferred_thread_reap_head = thread->next;
        if (!deferred_thread_reap_head) {
            deferred_thread_reap_tail = NULL;
        }

        reclaimed_pages = thread->deferred_reap_page_count;
        thread->next = NULL;
        thread->is_reap_queued = 0;
        thread->deferred_reap_page_count = 0;
    }

    if (reclaimed_pages > deferred_reap_pending_pages) {
        deferred_reap_pending_pages = 0;
    } else {
        deferred_reap_pending_pages -= reclaimed_pages;
    }

    StThread_UnlockPreemption();

    if (process) {
        struct StThread *main_thread = process->main_thread;

        if (main_thread && main_thread->is_dying) {
            process->main_thread = NULL;
            finalize_thread_storage(main_thread);
        }

        StProcess_FinalizeRemove(process);
        return reclaimed_pages;
    }

    if (thread) {
        finalize_thread_storage(thread);
        return reclaimed_pages;
    }

    return 0;
}

static void relieve_deferred_reap_pressure(St_PageCount reserved_pages)
{
    while (deferred_reap_pending_pages && deferred_reap_needs_pressure_relief(reserved_pages)) {
        if (!reap_one_deferred_item()) {
            break;
        }
    }

    while (deferred_reap_needs_pressure_relief(reserved_pages)) {
        if (!StThreadP_ReclaimCachedKernelStacks(STRATA_KSTACK_PAGE_COUNT)) {
            if (!StMmP_ReclaimCachedPageTableFrames(STRATA_KSTACK_PAGE_COUNT)) {
                break;
            }
        }
    }
}

StStatus StThread_Init(struct StThread **main_thread __out)
{
    StStatus status;
    struct StThread *main_th = NULL;
    int added_thread_to_scheduler = 0;

    status = allocate_thread_object(&main_th);
    if (!CHECK_SUCCESS(status)) goto has_error;

    main_th->id = 0;
    main_th->state = THREAD_STATE_RUNNING;
    main_th->type = THREAD_TYPE_MAIN;

    status = StScheduler_AddThread(main_th);
    if (!CHECK_SUCCESS(status)) goto has_error;
    added_thread_to_scheduler = 1;

    status = StScheduler_SwitchCurrentThread(main_th);
    if (!CHECK_SUCCESS(status)) goto has_error;

    *main_thread = main_th;

    return STATUS_SUCCESS;

has_error:
    if (added_thread_to_scheduler) {
        StScheduler_RemoveThread(main_th);
    }

    if (main_th) {
        free_thread_object(main_th);
    }

    return status;
}

void StThread_LockPreemption(void)
{
    atomic_uint_fast32_t *depth_ptr = get_preemption_disable_depth_ptr();
    uint32_t depth = atomic_fetch_add(depth_ptr, 1);

    if (depth < 0) {
        St_Panic(STATUS_CONFLICTING_STATE, "preemption enable underflow");
    }
}

void StThread_UnlockPreemption(void)
{
    atomic_uint_fast32_t *depth_ptr = get_preemption_disable_depth_ptr();
    uint32_t depth = atomic_fetch_sub(depth_ptr, 1);

    if (depth < 1) {
        St_Panic(STATUS_CONFLICTING_STATE, "preemption enable underflow");
    }
}

int StThread_IsPreemptionEnabled(void)
{
    atomic_uint_fast32_t *depth_ptr = get_preemption_disable_depth_ptr();
    return atomic_load(depth_ptr) == 0;
}

void StThread_RunDeferredReap(St_PageCount page_budget)
{
    St_PageCount reaped_pages = 0;

    if (page_budget == 0) {
        relieve_deferred_reap_pressure(0);
        return;
    }

    while (deferred_reap_pending_pages && reaped_pages < page_budget) {
        St_PageCount reclaimed_pages = reap_one_deferred_item();
        if (!reclaimed_pages) break;
        reaped_pages += reclaimed_pages;
    }
}

StStatus StThread_CreateKernel(StThread_EntryFunction entry __in, struct StThread **threadout __out)
{
    static StThread_Id new_thread_id = (StThread_Id)1;

    StStatus status;
    struct StThread *th = NULL;
    int stack_allocated = 0;
    int added_thread_to_scheduler = 0;
    uint32_t prev_thread_count;

    relieve_deferred_reap_pressure(STRATA_KSTACK_PAGE_COUNT);

    StThread_LockPreemption();

    /* create thread object */
    status = allocate_thread_object(&th);
    if (!CHECK_SUCCESS(status)) goto has_error;

    th->id = new_thread_id++;
    th->state = THREAD_STATE_PENDING;
    th->type = THREAD_TYPE_KERNEL;

    StThreadP_InitializePlatformData(th);

    /* prepare stack */
    th->kmode_stack_page_count = STRATA_KSTACK_PAGE_COUNT;
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

    prev_thread_count = atomic_fetch_add(&thread_count, 1);

    LOG_DEBUG(
        LM_CAT_UNCLASSIFIED,
        "created kernel thread #%d (active %" PRId32 ")\n",
        th->id,
        prev_thread_count + 1
    );

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
        free_thread_object(th);
    }

    StThread_UnlockPreemption();

    return status;
}

StStatus StThread_CreateUserMain(
    struct StProcess *process __in,
    uintptr_t entry __in,
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
    uint32_t prev_thread_count;

    relieve_deferred_reap_pressure(STRATA_KSTACK_PAGE_COUNT + STRATA_USTACK_PAGE_COUNT);

    StThread_LockPreemption();

    /* create thread object */
    status = allocate_thread_object(&th);
    if (!CHECK_SUCCESS(status)) goto has_error;

    th->id = new_thread_id++;
    th->state = THREAD_STATE_PENDING;
    th->type = THREAD_TYPE_USER;
    th->process = process;
    th->is_main = 1;

    StThreadP_InitializePlatformData(th);

    th->umode_entry = entry;

    /* prepare user stack */
    th->umode_stack_page_count = STRATA_USTACK_PAGE_COUNT;

    status = StThreadP_AllocateThreadUserStack(th);
    if (!CHECK_SUCCESS(status)) goto has_error;
    ustack_allocated = 1;

    status = StThreadP_SetupThreadUserStack(th, arg_count, args, env_count, envs);
    if (!CHECK_SUCCESS(status)) goto has_error;

    /* prepare kernel stack */
    th->kmode_stack_page_count = STRATA_KSTACK_PAGE_COUNT;

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

    prev_thread_count = atomic_fetch_add(&thread_count, 1);

    LOG_DEBUG(
        LM_CAT_UNCLASSIFIED,
        "created user thread #%d (active %" PRId32 ")\n",
        th->id,
        prev_thread_count + 1
    );

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
        free_thread_object(th);
    }

    StThread_UnlockPreemption();

    return status;
}

StStatus StThread_Remove(struct StThread *th)
{
    uint32_t prev_thread_count;

    if (th->type == THREAD_TYPE_MAIN) return STATUS_INVALID_THREAD;
    if (th->state != THREAD_STATE_FINISHED) return STATUS_THREAD_NOT_FINISHED;
    if (th->is_reap_queued) return STATUS_SUCCESS;

    th->is_dying = 1;

    StThread_LockPreemption();

    prev_thread_count = atomic_fetch_sub(&thread_count, 1);

    LOG_DEBUG(
        LM_CAT_UNCLASSIFIED,
        "removing thread #%d (active %" PRId32 ")\n",
        th->id,
        prev_thread_count - 1
    );

    remove_thread_from_process(th);
    StScheduler_RemoveThread(th);

    StThread_UnlockPreemption();

    if (th->type == THREAD_TYPE_USER && th->is_main) {
        if (th->process && !th->process->is_dying) {
            StProcess_BeginRemove(th->process);
        }

        if (th->process) {
            enqueue_process_for_reap(th->process);
            relieve_deferred_reap_pressure(0);
            return STATUS_SUCCESS;
        }
    }

    enqueue_thread_for_reap(th);
    relieve_deferred_reap_pressure(0);

    return STATUS_SUCCESS;
}

StStatus StThread_GetCount(uint32_t *count __out)
{
    *count = atomic_load(&thread_count);

    return STATUS_SUCCESS;
}

StStatus StThread_GetRuntime(struct StThread *thread __in, uint64_t *runtime_us __out)
{
    struct StThread *current_thread;
    uint64_t now_us;
    uint64_t total_us;
    StStatus status;

    if (!thread || !runtime_us) return STATUS_INVALID_VALUE;

    StThread_LockPreemption();

    status = StScheduler_GetCurrentThread(&current_thread);
    if (!CHECK_SUCCESS(status)) {
        StThread_UnlockPreemption();
        return status;
    }

    now_us = StTimeP_GetUptimeMicroseconds();
    total_us = thread->runtime_total_us;

    if (thread == current_thread && thread->last_scheduled_in_us &&
        now_us > thread->last_scheduled_in_us) {
        total_us += now_us - thread->last_scheduled_in_us;
    }

    *runtime_us = total_us;
    StThread_UnlockPreemption();

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

    if (thread->state == THREAD_STATE_FINISHED) {
        StScheduler_RequestMaintain();
    }

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
    StScheduler_RequestMaintain();

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "thread #%d finished\n", current_thread->id);

    StThread_Yield();

    for (;;) {
    }
}
