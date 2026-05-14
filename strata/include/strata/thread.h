#ifndef __STRATA_THREAD_H__
#define __STRATA_THREAD_H__

#include <strata/plat/thread.h>

#include <strata/compiler.h>
#include <strata/ref_control.h>
#include <strata/mm/owner.h>
#include <strata/mm/types.h>
#include <strata/status.h>

struct StProcess;
struct StScheduler;
struct StThread;

#ifndef __STRATA_PROCESS_REFS_DEFINED__
#    define __STRATA_PROCESS_REFS_DEFINED__
typedef struct StProcess *StProcess_StrongRef __ref_strong;
typedef struct StProcess *StProcess_WeakRef __ref_weak;
typedef struct StProcess *StProcess_BorrowedRef __ref_borrowed;
typedef struct StProcess *StProcess_InternalRef __ref_internal;
#endif

#ifndef __STRATA_THREAD_REFS_DEFINED__
#    define __STRATA_THREAD_REFS_DEFINED__
typedef struct StThread *StThread_StrongRef __ref_strong;
typedef struct StThread *StThread_WeakRef __ref_weak;
typedef struct StThread *StThread_BorrowedRef __ref_borrowed;
typedef struct StThread *StThread_InternalRef __ref_internal;
#endif

typedef void (*StThread_EntryFunction)(StThread_BorrowedRef);

enum StThread_State {
    THREAD_STATE_PENDING = 0,
    THREAD_STATE_RUNNING,
    THREAD_STATE_BLOCKING,
    THREAD_STATE_WAITING,
    THREAD_STATE_SLEEPING,
    THREAD_STATE_FINISHED,
};

enum StThread_Type {
    THREAD_TYPE_MAIN = 0,
    THREAD_TYPE_KERNEL,
    THREAD_TYPE_MODULE,
    THREAD_TYPE_USER,
};

typedef int StThread_Id __nocast;
typedef uint32_t StThread_CreateFlags __nocast;

#define TCF_DEFAULT  ((StThread_CreateFlags)0x00000000)
#define TCF_DETACHED ((StThread_CreateFlags)0x00000001)

struct StThread {
    struct StRefControlBlock ref_control;

    StThread_InternalRef next;
    StThread_InternalRef mutex_blocking_next;
    StThread_InternalRef process_next;

    StThread_Id id;
    StProcess_StrongRef process;
    enum StThread_State state;
    enum StThread_Type type;
    int is_detached;
    int is_main;

    struct StThreadP_PlatformData platform_data;

    size_t kmode_stack_page_count;
    St_VirtPage kmode_stack_base_vpn;
    void *kmode_stack_ptr;
    StThread_EntryFunction kmode_entry;

    size_t umode_stack_page_count;
    St_VirtPage umode_stack_base_vpn;
    uintptr_t umode_stack_ptr;
    uintptr_t umode_entry;

    StThread_StrongRef *wait_list;
    uint64_t wait_timeout_ms;
    StStatus wait_status;
    int wait_count;

    uint64_t sleep_until_uptime_ns;

    /*
     * Scheduler metadata:
     * - sched_pass: monotonically increasing fairness key.
     * - sched_run_count: number of slices selected for this thread.
     */
    uint64_t sched_pass;
    uint64_t sched_run_count;
    uint64_t runtime_total_ns;
    uint64_t last_scheduled_in_ns;

};

StStatus StThread_Init(StThread_StrongRef *main_thread __out);

void StThread_LockPreemption(void);
void StThread_UnlockPreemption(void);

int StThread_IsPreemptionEnabled(void);

StStatus StThread_CreateKernel(
    StThread_EntryFunction entry __in,
    StThread_CreateFlags flags __in,
    StThread_StrongRef *threadout __out_optional
);
StStatus StThread_CreateUserMain(
    StProcess_StrongRef process __in,
    uintptr_t entry __in,
    int arg_count __in,
    const char *const *args __in,
    int env_count __in,
    const char *const *envs __in,
    StThread_StrongRef *threadout __out
);
StStatus StThread_CreateUser(
    StProcess_StrongRef process __in, uintptr_t entry __in, StThread_StrongRef *threadout __out
);
StStatus StThread_Acquire(StThread_WeakRef thread __in, StThread_StrongRef *threadout __out);
StStatus StThread_AcquireInternal(
    StThread_InternalRef thread __in, StThread_StrongRef *threadout __out
);
void StThread_Release(StThread_StrongRef thread __inout);
StStatus StThread_Remove(StThread_StrongRef thread __in);

void StThread_GetCount(uint32_t *count __out);
StStatus StThread_GetRuntime(StThread_StrongRef thread __in, uint64_t *runtime_ns __out);
void StThread_RunDeferredReap(St_PageCount page_budget __in);

StStatus StThread_Detach(StThread_StrongRef thread __in);
StStatus StThread_Wait(StThread_StrongRef *list __in, int count __in, uint64_t timeout_ms __in);

void StThread_Sleep(uint64_t timeout_ms __in);

void StThread_Yield(void);

__noreturn void StThread_Exit(void);

#endif  // __STRATA_THREAD_H__
