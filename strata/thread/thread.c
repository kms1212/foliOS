#include "config.h"

#include <strata/thread.h>

#include <assert.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdint.h>

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
#include <strata/process_refs.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread_refs.h>

#define MODULE_NAME                                   "thread"
#define THREAD_DEFERRED_REAP_MAX_PENDING_PAGES        ((St_PageCount)512)
#define THREAD_DEFERRED_REAP_LOW_FREE_FRAME_WATERMARK ((St_PageCount)4096)

static atomic_uint_fast32_t preemption_disable_depth_early = 0;
static atomic_uint_fast32_t thread_count = 0;
static StThread_InternalRef deferred_thread_reap_head = NULL;
static StThread_InternalRef deferred_thread_reap_tail = NULL;
static StProcess_InternalRef deferred_process_reap_head = NULL;
static StProcess_InternalRef deferred_process_reap_tail = NULL;
static St_PageCount deferred_reap_pending_pages = 0;

static StStatus allocate_thread_object(StThread_StrongRef *threadout)
{
    return StPool_AllocateClearAligned(
        sizeof(struct StThread),
        __builtin_ctzll((unsigned long long)__alignof__(struct StThread)),
        (void **)threadout
    );
}

static void free_thread_object(StThread_InternalRef thread)
{
    if (!thread) return;

    StThreadP_FreePlatformData(thread);
    StPool_Free(thread);
}

static inline atomic_uint_fast32_t *get_preemption_disable_depth_ptr(void)
{
    struct StCpuLocalP_Data *cpu_local = StCpuLocalP_GetData();
    if (cpu_local) return &cpu_local->preemption_disable_depth;
    return &preemption_disable_depth_early;
}

static void add_thread_to_process(StThread_InternalRef thread)
{
    StProcess_StrongRef process;

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

static void remove_thread_from_process(StThread_InternalRef thread)
{
    StProcess_StrongRef process;
    StThread_InternalRef prev;

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

static int wait_list_is_ready(StThread_StrongRef *list, int count)
{
    int ready = 1;

    for (int i = 0; i < count; i++) {
        if (!list[i]) continue;
        if (list[i]->state != THREAD_STATE_FINISHED) {
            ready = 0;
            continue;
        }

        list[i] = NULL;
    }

    return ready;
}

static uint64_t get_timeout_deadline_ns(uint64_t now_ns, uint64_t timeout_ms)
{
    if (timeout_ms == UINT64_MAX) return 0;
    if (timeout_ms > (UINT64_MAX - now_ns) / 1000000) return UINT64_MAX;

    return now_ns + (timeout_ms * 1000000);
}

static void finish_current_wait(StThread_InternalRef thread, StStatus status)
{
    thread->wait_list = NULL;
    thread->wait_count = 0;
    thread->wait_timeout_ms = 0;
    thread->wait_status = status;
    thread->sleep_until_uptime_ns = 0;
    thread->state = THREAD_STATE_RUNNING;
}

static St_PageCount get_thread_reap_page_count(StThread_InternalRef thread)
{
    St_PageCount pages = 0;

    if (!thread) return 0;

    pages += thread->kmode_stack_page_count;
    if (thread->type == THREAD_TYPE_USER) {
        pages += thread->umode_stack_page_count;
    }

    return pages;
}

static St_PageCount get_process_reap_page_count(StProcess_InternalRef process)
{
    St_PageCount pages = 1;

    if (!process) return 0;

    if (process->alloc_owner) {
        pages += process->alloc_owner->page_usage_count;
    }
    if (process->main_thread) {
        pages += get_thread_reap_page_count(process->main_thread);
    }

    return pages;
}

static int deferred_reap_needs_pressure_relief(St_PageCount reserved_pages)
{
    St_PageCount free_frames = 0;

    if (deferred_reap_pending_pages + reserved_pages > THREAD_DEFERRED_REAP_MAX_PENDING_PAGES) {
        return 1;
    }

    StPmm_GetFreeFrameCount(&free_frames);
    if (free_frames <= THREAD_DEFERRED_REAP_LOW_FREE_FRAME_WATERMARK) {
        return 1;
    }

    if (reserved_pages &&
        free_frames <= THREAD_DEFERRED_REAP_LOW_FREE_FRAME_WATERMARK + reserved_pages) {
        return 1;
    }

    return 0;
}

static void finalize_thread_storage(void *object)
{
    StThread_InternalRef thread = object;

    if (!thread) return;

    StThreadP_FreeThreadKernelStack(thread);

    if (thread->type == THREAD_TYPE_USER) {
        StThreadP_FreeThreadUserStack(thread);
    }

    if (thread->process) {
        StProcess_Release(thread->process);
        thread->process = NULL;
    }

    free_thread_object(thread);
}

static void acquire_thread(StThread_InternalRef thread)
{
    assert(thread);

    StRefControlBlock_Acquire(&thread->ref_control);
}

static void release_thread(StThread_StrongRef thread)
{
    assert(thread);

    (void)StRefControlBlock_Release(&thread->ref_control);
}

static StStatus acquire_thread_ref(StThread_InternalRef thread, StThread_StrongRef *threadout)
{
    assert(threadout);

    if (!thread) return STATUS_INVALID_VALUE;
    if (StRefControlBlock_IsDying(&thread->ref_control)) return STATUS_ENTRY_NOT_FOUND;

    acquire_thread(thread);
    *threadout = (StThread_StrongRef)thread;

    return STATUS_SUCCESS;
}

StStatus StThread_Acquire(StThread_WeakRef thread __in, StThread_StrongRef *threadout __out)
{
    assert(threadout);

    return acquire_thread_ref((StThread_InternalRef)thread, threadout);
}

StStatus StThread_AcquireInternal(
    StThread_InternalRef thread __in, StThread_StrongRef *threadout __out
)
{
    assert(threadout);

    return acquire_thread_ref(thread, threadout);
}

void StThread_Release(StThread_StrongRef thread __inout)
{
    assert(thread);

    release_thread(thread);
}

static void enqueue_thread_for_reap(StThread_StrongRef thread)
{
    if (!thread || StRefControlBlock_IsReapQueued(&thread->ref_control)) return;

    thread->next = NULL;
    StRefControlBlock_MarkReapQueued(&thread->ref_control);
    thread->ref_control.deferred_reap_page_count =
        get_thread_reap_page_count((StThread_InternalRef)thread);

    StThread_LockPreemption();

    if (!deferred_thread_reap_head) {
        deferred_thread_reap_head = deferred_thread_reap_tail = (StThread_InternalRef)thread;
    } else {
        deferred_thread_reap_tail->next = (StThread_InternalRef)thread;
        deferred_thread_reap_tail = (StThread_InternalRef)thread;
    }

    deferred_reap_pending_pages += thread->ref_control.deferred_reap_page_count;

    StThread_UnlockPreemption();
}

static void enqueue_process_for_reap(StProcess_StrongRef process)
{
    if (!process || StRefControlBlock_IsReapQueued(&process->ref_control)) return;

    process->next = NULL;
    StRefControlBlock_MarkReapQueued(&process->ref_control);
    process->ref_control.deferred_reap_page_count =
        get_process_reap_page_count((StProcess_InternalRef)process);
    if (process->main_thread) {
        StRefControlBlock_MarkReapQueued(&process->main_thread->ref_control);
    }

    StThread_LockPreemption();

    if (!deferred_process_reap_head) {
        deferred_process_reap_head = deferred_process_reap_tail = (StProcess_InternalRef)process;
    } else {
        deferred_process_reap_tail->next = (StProcess_InternalRef)process;
        deferred_process_reap_tail = (StProcess_InternalRef)process;
    }

    deferred_reap_pending_pages += process->ref_control.deferred_reap_page_count;

    StThread_UnlockPreemption();
}

static St_PageCount reap_one_deferred_item(void)
{
    St_PageCount reclaimed_pages = 0;
    StProcess_InternalRef process = NULL;
    StThread_InternalRef thread = NULL;

    StThread_LockPreemption();

    if (deferred_process_reap_head) {
        process = deferred_process_reap_head;
        deferred_process_reap_head = process->next;
        if (!deferred_process_reap_head) {
            deferred_process_reap_tail = NULL;
        }

        reclaimed_pages = process->ref_control.deferred_reap_page_count;
        process->next = NULL;
        StRefControlBlock_ClearReapQueued(&process->ref_control);
        process->ref_control.deferred_reap_page_count = 0;
    } else if (deferred_thread_reap_head) {
        thread = deferred_thread_reap_head;
        deferred_thread_reap_head = thread->next;
        if (!deferred_thread_reap_head) {
            deferred_thread_reap_tail = NULL;
        }

        reclaimed_pages = thread->ref_control.deferred_reap_page_count;
        thread->next = NULL;
        StRefControlBlock_ClearReapQueued(&thread->ref_control);
        thread->ref_control.deferred_reap_page_count = 0;
    }

    if (reclaimed_pages > deferred_reap_pending_pages) {
        deferred_reap_pending_pages = 0;
    } else {
        deferred_reap_pending_pages -= reclaimed_pages;
    }

    StThread_UnlockPreemption();

    if (process) {
        StThread_InternalRef main_thread = process->main_thread;

        if (main_thread && StRefControlBlock_IsDying(&main_thread->ref_control)) {
            process->main_thread = NULL;
            release_thread((StThread_StrongRef)main_thread);
        }

        StProcess_Release((StProcess_StrongRef)process);
        return reclaimed_pages;
    }

    if (thread) {
        release_thread((StThread_StrongRef)thread);
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

StStatus StThread_Init(StThread_StrongRef *main_thread __out)
{
    assert(main_thread);

    StStatus status;
    StThread_StrongRef main_th = NULL;
    int added_thread_to_scheduler = 0;

    status = allocate_thread_object(&main_th);
    if (!CHECK_SUCCESS(status)) goto has_error;

    main_th->id = 0;
    main_th->state = THREAD_STATE_RUNNING;
    main_th->type = THREAD_TYPE_MAIN;
    StRefControlBlock_Init(&main_th->ref_control, 1, main_th, NULL);

    status = StScheduler_AddThread((StThread_InternalRef)main_th);
    if (!CHECK_SUCCESS(status)) goto has_error;
    added_thread_to_scheduler = 1;

    status = StScheduler_SwitchCurrentThread((StThread_InternalRef)main_th);
    if (!CHECK_SUCCESS(status)) goto has_error;

    *main_thread = main_th;

    return STATUS_SUCCESS;

has_error:
    if (added_thread_to_scheduler) {
        StScheduler_RemoveThread((StThread_InternalRef)main_th);
    }

    if (main_th) {
        free_thread_object((StThread_InternalRef)main_th);
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

StStatus StThread_CreateKernel(
    StThread_EntryFunction entry __in,
    StThread_CreateFlags flags __in,
    StThread_StrongRef *threadout __out_optional
)
{
    static StThread_Id new_thread_id = (StThread_Id)1;

    StStatus status;
    StThread_StrongRef th = NULL;
    int stack_allocated = 0;
    int added_thread_to_scheduler = 0;
    uint32_t prev_thread_count;

    if (flags & ~TCF_DETACHED) return STATUS_INVALID_VALUE;
    if (!(flags & TCF_DETACHED) && !threadout) return STATUS_INVALID_VALUE;

    relieve_deferred_reap_pressure(STRATA_KSTACK_PAGE_COUNT);

    StThread_LockPreemption();

    /* create thread object */
    status = allocate_thread_object(&th);
    if (!CHECK_SUCCESS(status)) goto has_error;

    th->id = new_thread_id++;
    th->state = THREAD_STATE_PENDING;
    th->type = THREAD_TYPE_KERNEL;
    th->is_detached = (flags & TCF_DETACHED) != 0;
    StRefControlBlock_Init(&th->ref_control, 1, th, finalize_thread_storage);
    if (!(flags & TCF_DETACHED)) {
        acquire_thread((StThread_InternalRef)th);
    }

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
    status = StScheduler_AddThread((StThread_InternalRef)th);
    if (!CHECK_SUCCESS(status)) goto has_error;
    added_thread_to_scheduler = 1;

    prev_thread_count = atomic_fetch_add(&thread_count, 1);

    LOG_DEBUG(
        LM_CAT_UNCLASSIFIED,
        "created kernel thread #%d (active %" PRId32 ")\n",
        th->id,
        prev_thread_count + 1
    );

    if (threadout) *threadout = th;

    StThread_UnlockPreemption();

    return STATUS_SUCCESS;

has_error:
    if (th && added_thread_to_scheduler) {
        StScheduler_RemoveThread((StThread_InternalRef)th);
    }

    if (th && stack_allocated) {
        StThreadP_FreeThreadKernelStack(th);
    }

    if (th) {
        free_thread_object((StThread_InternalRef)th);
    }

    StThread_UnlockPreemption();

    return status;
}

StStatus StThread_CreateUserMain(
    StProcess_StrongRef process __in,
    uintptr_t entry __in,
    int arg_count __in,
    const char *const *args __in,
    int env_count __in,
    const char *const *envs __in,
    StThread_StrongRef *threadout __out
)
{
    assert(threadout);

    static StThread_Id new_thread_id = (StThread_Id)16384;

    StStatus status;
    StThread_StrongRef th = NULL;
    int added_thread_to_scheduler = 0;
    int kstack_allocated = 0;
    int ustack_allocated = 0;
    int added_thread_to_process = 0;
    int acquired_process = 0;
    int assigned_main_thread = 0;
    uint32_t prev_thread_count;

    assert(process);
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
    StRefControlBlock_Init(&th->ref_control, 1, th, finalize_thread_storage);
    acquire_thread((StThread_InternalRef)th);

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

    StProcess_Acquire(process);
    acquired_process = 1;

    if (process->main_thread) {
        status = STATUS_CONFLICTING_STATE;
        goto has_error;
    }
    process->main_thread = (StThread_InternalRef)th;
    assigned_main_thread = 1;

    /* add thread object to list */
    status = StScheduler_AddThread((StThread_InternalRef)th);
    if (!CHECK_SUCCESS(status)) goto has_error;
    added_thread_to_scheduler = 1;

    add_thread_to_process((StThread_InternalRef)th);
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
    if (assigned_main_thread && process->main_thread == (StThread_InternalRef)th) {
        process->main_thread = NULL;
    }

    if (th && added_thread_to_process) {
        remove_thread_from_process((StThread_InternalRef)th);
    }

    if (th && added_thread_to_scheduler) {
        StScheduler_RemoveThread((StThread_InternalRef)th);
    }

    if (acquired_process) {
        StProcess_Release(process);
    }

    if (th && ustack_allocated) {
        StThreadP_FreeThreadUserStack(th);
    }

    if (th && kstack_allocated) {
        StThreadP_FreeThreadKernelStack(th);
    }

    if (th) {
        free_thread_object((StThread_InternalRef)th);
    }

    StThread_UnlockPreemption();

    return status;
}

StStatus StThread_Remove(StThread_StrongRef th)
{
    uint32_t prev_thread_count;
    int release_join_ref;

    StThread_LockPreemption();

    if (th->type == THREAD_TYPE_MAIN) {
        StThread_UnlockPreemption();
        return STATUS_INVALID_THREAD;
    }
    if (StRefControlBlock_IsReapQueued(&th->ref_control)) {
        StThread_UnlockPreemption();
        return STATUS_SUCCESS;
    }
    if (th->state != THREAD_STATE_FINISHED) {
        StThread_UnlockPreemption();
        return STATUS_THREAD_NOT_FINISHED;
    }

    release_join_ref = !th->is_detached;
    StRefControlBlock_MarkDying(&th->ref_control);

    prev_thread_count = atomic_fetch_sub(&thread_count, 1);

    LOG_DEBUG(
        LM_CAT_UNCLASSIFIED,
        "removing thread #%d (active %" PRId32 ")\n",
        th->id,
        prev_thread_count - 1
    );

    remove_thread_from_process((StThread_InternalRef)th);
    StScheduler_RemoveThread((StThread_InternalRef)th);

    StThread_UnlockPreemption();

    if (th->type == THREAD_TYPE_USER && th->is_main) {
        if (th->process && !StRefControlBlock_IsDying(&th->process->ref_control)) {
            StProcess_BeginRemove(th->process);
        }

        if (th->process) {
            enqueue_process_for_reap(th->process);
            relieve_deferred_reap_pressure(0);
            if (release_join_ref) {
                release_thread(th);
            }
            return STATUS_SUCCESS;
        }
    }

    enqueue_thread_for_reap(th);
    relieve_deferred_reap_pressure(0);
    if (release_join_ref) {
        release_thread(th);
    }

    return STATUS_SUCCESS;
}

void StThread_GetCount(uint32_t *count __out)
{
    assert(count);

    *count = atomic_load(&thread_count);
}

StStatus StThread_GetRuntime(StThread_StrongRef thread __in, uint64_t *runtime_ns __out)
{
    assert(runtime_ns);

    StThread_InternalRef current_thread;
    uint64_t now_ns;
    uint64_t total_ns;
    StStatus status;

    if (!thread) return STATUS_INVALID_VALUE;

    StThread_LockPreemption();

    status = StScheduler_GetCurrentThread(&current_thread);
    if (!CHECK_SUCCESS(status)) {
        StThread_UnlockPreemption();
        return status;
    }

    StTimeP_GetUptimeNanoseconds(&now_ns);
    total_ns = thread->runtime_total_ns;

    if (thread == current_thread && thread->last_scheduled_in_ns &&
        now_ns > thread->last_scheduled_in_ns) {
        total_ns += now_ns - thread->last_scheduled_in_ns;
    }

    *runtime_ns = total_ns;
    StThread_UnlockPreemption();

    return STATUS_SUCCESS;
}

StStatus StThread_Detach(StThread_StrongRef thread)
{
    if (!thread) return STATUS_INVALID_VALUE;

    if (thread->is_detached) return STATUS_SUCCESS;

    thread->is_detached = 1;
    release_thread(thread);

    if (thread->state == THREAD_STATE_FINISHED) {
        StScheduler_RequestMaintain();
    }

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "detaching thread #%d\n", thread->id);

    return STATUS_SUCCESS;
}

StStatus StThread_Wait(StThread_StrongRef *list __in, int count __in, uint64_t timeout_ms __in)
{
    StStatus status;
    StThread_InternalRef current_thread;
    uint64_t deadline_ns;

    status = StScheduler_GetCurrentThread(&current_thread);
    if (!CHECK_SUCCESS(status)) return status;

    if (count < 0 || (!list && count > 0)) return STATUS_INVALID_VALUE;
    if (count == 0) return STATUS_SUCCESS;

    for (int i = 0; i < count; i++) {
        if (!list[i]) continue;
        if (list[i]->is_detached) return STATUS_CONFLICTING_STATE;
    }

    if (wait_list_is_ready(list, count)) {
        return STATUS_SUCCESS;
    }

    StThread_LockPreemption();

    if (timeout_ms == 0) {
        StThread_UnlockPreemption();
        return STATUS_TIMER_EXPIRED;
    }

    StTimeP_GetUptimeNanoseconds(&deadline_ns);
    deadline_ns = get_timeout_deadline_ns(deadline_ns, timeout_ms);
    current_thread->wait_list = list;
    current_thread->wait_count = count;
    current_thread->wait_timeout_ms = timeout_ms;
    current_thread->wait_status = STATUS_PENDING;
    current_thread->sleep_until_uptime_ns = deadline_ns;
    current_thread->state = THREAD_STATE_WAITING;

    StThread_UnlockPreemption();

    for (;;) {
        uint64_t now_ns;

        StThread_LockPreemption();

        if (current_thread->state != THREAD_STATE_WAITING) {
            status = current_thread->wait_status;
            if (status == STATUS_PENDING) {
                if (!wait_list_is_ready(current_thread->wait_list, current_thread->wait_count)) {
                    current_thread->state = THREAD_STATE_WAITING;
                    StThread_UnlockPreemption();
                    continue;
                }
                status = STATUS_SUCCESS;
            }
            current_thread->wait_list = NULL;
            current_thread->wait_count = 0;
            current_thread->wait_timeout_ms = 0;
            current_thread->sleep_until_uptime_ns = 0;
            current_thread->wait_status = STATUS_SUCCESS;
            StThread_UnlockPreemption();
            return status;
        }

        if (wait_list_is_ready(current_thread->wait_list, current_thread->wait_count)) {
            finish_current_wait(current_thread, STATUS_SUCCESS);
            StThread_UnlockPreemption();
            return STATUS_SUCCESS;
        }

        StTimeP_GetUptimeNanoseconds(&now_ns);
        if (timeout_ms != UINT64_MAX && now_ns >= deadline_ns) {
            finish_current_wait(current_thread, STATUS_TIMER_EXPIRED);
            StThread_UnlockPreemption();
            return STATUS_TIMER_EXPIRED;
        }

        StThread_UnlockPreemption();

        if (StScheduler_CheckHasOtherRunnableThread()) {
            StThread_Yield();
        } else {
            StThreadP_IdleUntilInterrupt();
        }
    }

    return STATUS_SUCCESS;
}

void StThread_Sleep(uint64_t timeout_ms __in)
{
    StStatus status;
    StThread_InternalRef current_thread;
    uint64_t deadline_ns;

    status = StScheduler_GetCurrentThread(&current_thread);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "cannot get current thread for sleep");
    }

    StTimeP_GetUptimeNanoseconds(&deadline_ns);
    deadline_ns += timeout_ms * 1000000;

    StThread_LockPreemption();

    current_thread->state = THREAD_STATE_SLEEPING;
    current_thread->sleep_until_uptime_ns = deadline_ns;

    StThread_UnlockPreemption();

    for (;;) {
        uint64_t now_ns;

        StThread_LockPreemption();

        if (current_thread->state != THREAD_STATE_SLEEPING) {
            StThread_UnlockPreemption();
            break;
        }

        StTimeP_GetUptimeNanoseconds(&now_ns);
        if (now_ns >= deadline_ns) {
            current_thread->sleep_until_uptime_ns = 0;
            current_thread->state = THREAD_STATE_RUNNING;
            StThread_UnlockPreemption();
            break;
        }

        StThread_UnlockPreemption();

        if (StScheduler_CheckHasOtherRunnableThread()) {
            StThread_Yield();
        } else {
            StThreadP_IdleUntilInterrupt();
        }
    }

}

void StThread_Yield(void)
{
    StThreadP_Yield();
}

__noreturn void StThread_Exit(void)
{
    StStatus status;
    StThread_InternalRef current_thread;

    status = StScheduler_GetCurrentThread(&current_thread);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "cannot get current thread");
    }

    if (current_thread->type == THREAD_TYPE_MAIN) {
        St_Panic(STATUS_INVALID_THREAD, "cannot exit from main thread");
    }

    StThread_LockPreemption();
    current_thread->state = THREAD_STATE_FINISHED;
    StScheduler_RequestMaintain();
    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "thread #%d finished\n", current_thread->id);
    StThread_UnlockPreemption();

    StThread_Yield();

    for (;;) {
    }
}
