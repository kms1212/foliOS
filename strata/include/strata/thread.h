#ifndef __STRATA_THREAD_H__
#define __STRATA_THREAD_H__

#include <strata/plat/thread.h>

#include <strata/compiler.h>
#include <strata/mm/owner.h>
#include <strata/mm/types.h>
#include <strata/status.h>

struct StThread;
struct StProcess;
struct StScheduler;

typedef void (*StThread_EntryFunction)(struct StThread *);

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

struct StThread {
    struct StThread *next;
    struct StThread *mutex_blocking_next;
    struct StThread *process_next;

    StThread_Id id;
    struct StProcess *process;
    enum StThread_State status;
    enum StThread_Type type;
    int is_detached;
    int is_main;
    int is_dying;

    struct StThreadP_PlatformData platform_data;

    size_t kmode_stack_page_count;
    St_VirtPage kmode_stack_base_vpn;
    void *kmode_stack_ptr;
    StThread_EntryFunction kmode_entry;

    size_t umode_stack_page_count;
    St_VirtPage umode_stack_base_vpn;
    uintptr_t umode_stack_ptr;
    uintptr_t umode_entry;

    struct StThread **wait_list;
    int wait_count;
    int wait_timeout_ms;

    uint64_t sleep_until_uptime_us;

    struct StMm_AllocationOwner alloc_owner;
};

StStatus StThread_Init(struct StThread **main_thread __out);

void StThread_LockPreemption(void);
void StThread_UnlockPreemption(void);

int StThread_IsPreemptionEnabled(void);

StStatus StThread_CreateKernel(
    StThread_EntryFunction entry __in,
    St_PageCount stack_page_count __in,
    struct StThread **threadout __out
);
StStatus StThread_CreateUser(
    struct StProcess *process __in,
    uintptr_t entry __in,
    St_PageCount kstack_page_count __in,
    St_PageCount ustack_page_count __in,
    struct StThread **threadout __out
);
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
);
StStatus StThread_Remove(struct StThread *thread __in);

StStatus StThread_Detach(struct StThread *thread __in);
StStatus StThread_Wait(struct StThread **list __in, int count __in, int timeout_ms __in);

StStatus StThread_Sleep(int timeout_ms __in);

void StThread_Yield(void);

__noreturn void StThread_Exit(void);

#endif  // __STRATA_THREAD_H__
