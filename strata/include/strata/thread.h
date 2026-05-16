#ifndef __STRATA_THREAD_H__
#define __STRATA_THREAD_H__

#include <stdint.h>

#include <strata/plat/thread.h>

#include <strata/compiler.h>
#include <strata/mm/allocation_owner.h>
#include <strata/mm/types.h>
#include <strata/process_refs.h>
#include <strata/ref_control.h>
#include <strata/status.h>
#include <strata/thread_refs.h>

struct StScheduler;

/**
 * Thread entry point used by kernel-mode threads.
 *
 * The callee receives a borrowed view of the running thread. It must not store
 * the pointer beyond the entry call unless it first acquires a strong reference
 * through the thread API.
 */
typedef void (*StThread_EntryFunction)(StThread_BorrowedRef);

/** Scheduler-visible thread state. */
enum StThread_State {
    /** Created but not yet selected by the scheduler. */
    THREAD_STATE_PENDING = 0,
    /** Runnable or currently running. */
    THREAD_STATE_RUNNING,
    /** Blocked on a synchronization primitive. */
    THREAD_STATE_BLOCKING,
    /** Waiting for one or more threads. */
    THREAD_STATE_WAITING,
    /** Sleeping until a timeout expires. */
    THREAD_STATE_SLEEPING,
    /** Finished execution; removal/reap may still be pending. */
    THREAD_STATE_FINISHED,
};

/** Thread execution domain. */
enum StThread_Type {
    THREAD_TYPE_MAIN = 0,
    THREAD_TYPE_KERNEL,
    THREAD_TYPE_MODULE,
    THREAD_TYPE_USER,
};

typedef int StThread_Id __nocast;
typedef uint32_t StThread_CreateFlags __nocast __flagset(thread_create);

/** Normal joinable thread creation. */
#define TCF_DEFAULT ((StThread_CreateFlags)0x00000000)
/** Create the thread already detached; the creator receives no join ref. */
#define TCF_DETACHED ((StThread_CreateFlags)0x00000001)

/** Infinite timeout sentinel for StThread_Wait. */
#define THREAD_WAIT_INFINITE ((uint64_t)UINT64_MAX)

/**
 * Ref-counted Strata thread object.
 *
 * Scheduler and process lists use InternalRef links. Public users should hold a
 * StrongRef while they need the object to remain alive. Finished detached
 * threads are reaped by scheduler/deferred cleanup once it is safe to destroy
 * their stacks and object storage.
 */
struct StThread {
    /** First-field ref control block used by StThread_Acquire/Release. */
    struct StRefControlBlock ref_control;

    /** Scheduler runqueue link. */
    StThread_InternalRef next;
    /** Mutex wait queue link. */
    StThread_InternalRef mutex_blocking_next;
    /** Owning process thread-list link. */
    StThread_InternalRef process_next;

    StThread_Id id;
    /** Owning process. Threads keep this strong while they exist. */
    StProcess_StrongRef process;
    enum StThread_State state;
    enum StThread_Type type;
    /** Nonzero after the join reference has been released/transferred. */
    int is_detached;
    /** Nonzero for the process main thread. */
    int is_main;

    struct StThreadP_PlatformData platform_data;

    /** Kernel stack size in pages. */
    St_PageCount kmode_stack_page_count;
    /** Base VPN of the kernel stack reservation. */
    St_VirtPage kmode_stack_base_vpn;
    /** Saved kernel stack pointer used by platform context switching. */
    void *kmode_stack_ptr;
    /** Kernel entry used for kernel-mode threads. */
    StThread_EntryFunction kmode_entry;

    /** User stack size in pages; zero for non-user threads. */
    St_PageCount umode_stack_page_count;
    /** Base VPN of the user stack reservation. */
    St_VirtPage umode_stack_base_vpn;
    /** User stack pointer passed to the initial user-mode frame. */
    uintptr_t umode_stack_ptr;
    /** User entry address for user/module threads. */
    uintptr_t umode_entry;

    /** Wait list owned by the waiting thread while in THREAD_STATE_WAITING. */
    StThread_StrongRef *wait_list;
    /** Wait timeout in milliseconds. */
    uint64_t wait_timeout_ms;
    /** Status reported when the wait completes. */
    StStatus wait_status;
    /** Number of entries in wait_list. */
    int wait_count;

    /** Absolute uptime deadline for sleeping threads. */
    uint64_t sleep_until_uptime_ns;

    /*
     * Scheduler metadata:
     * - sched_pass: monotonically increasing fairness key.
     * - sched_run_count: number of slices selected for this thread.
     */
    uint64_t sched_pass;
    uint64_t sched_run_count;
    /** Total scheduled runtime in nanoseconds. */
    uint64_t runtime_total_ns;
    /** Timestamp when the thread most recently became current. */
    uint64_t last_scheduled_in_ns;
};

/** Initialize the threading subsystem and publish the boot/main thread. */
StStatus StThread_Init(StThread_StrongRef *main_thread __out);

/** Disable preemption on the current CPU until the matching unlock. */
void StThread_LockPreemption(void);
/** Re-enable preemption after StThread_LockPreemption. */
void StThread_UnlockPreemption(void);

/** Return nonzero when the current execution context permits preemption. */
int StThread_IsPreemptionEnabled(void);

/**
 * Create a kernel thread.
 *
 * Joinable threads return a strong join reference in threadout. Detached
 * threads must be created with TCF_DETACHED; in that case threadout may be NULL
 * because ownership is transferred to scheduler/reap paths.
 */
StStatus StThread_CreateKernel(
    StThread_EntryFunction entry __in,
    StThread_CreateFlags flags __in,
    StThread_StrongRef *threadout __out_optional
);
/** Create the main user thread for a process and initialize argv/envp state. */
StStatus StThread_CreateUserMain(
    StProcess_StrongRef process __in,
    uintptr_t entry __in,
    int arg_count __in,
    const char *const *args __in,
    int env_count __in,
    const char *const *envs __in,
    StThread_StrongRef *threadout __out
);
/** Create a non-main user thread in an existing process. */
StStatus StThread_CreateUser(
    StProcess_StrongRef process __in, uintptr_t entry __in, StThread_StrongRef *threadout __out
);
/** Promote a weak thread reference to a strong reference if still alive. */
StStatus StThread_Acquire(StThread_WeakRef thread __in, StThread_StrongRef *threadout __out);
/** Promote an internal thread link to a strong reference if still safe. */
StStatus StThread_AcquireInternal(
    StThread_InternalRef thread __in, StThread_StrongRef *threadout __out
);
/** Release a strong thread reference. May finalize if this was the last ref. */
void StThread_Release(StThread_StrongRef thread __inout);
/** Remove a finished thread from scheduler/process ownership and queue reap. */
StStatus StThread_Remove(StThread_StrongRef thread __in);

/** Write the current live thread count. */
void StThread_GetCount(uint32_t *count __out);
/** Write accumulated runtime for a live thread. */
StStatus StThread_GetRuntime(StThread_StrongRef thread __in, uint64_t *runtime_ns __out);
/** Run deferred thread/process cleanup while respecting a page budget. */
void StThread_RunDeferredReap(St_PageCount page_budget __in);

/** Detach a joinable thread and release the join reference. */
StStatus StThread_Detach(StThread_StrongRef thread __in);
/** Wait until every thread in list finishes or timeout_ms expires. */
StStatus StThread_Wait(StThread_StrongRef *list __in, int count __in, uint64_t timeout_ms __in);

/** Put the current thread to sleep for timeout_ms. */
void StThread_Sleep(uint64_t timeout_ms __in);

/** Yield the current CPU to the scheduler. */
void StThread_Yield(void);

/** Mark the current thread finished and never return to its caller. */
__noreturn void StThread_Exit(void);

#endif  // __STRATA_THREAD_H__
