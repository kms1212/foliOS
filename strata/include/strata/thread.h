#ifndef __STRATA_THREAD_H__
#define __STRATA_THREAD_H__

#include <strata/plat/thread.h>

#include <strata/status.h>
#include <strata/compiler.h>
#include <strata/mm.h>

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
    struct StProcess *owner;
    enum StThread_State status;
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

    struct StThread **wait_list;
    int wait_count;
    int wait_timeout_ms;

    uint64_t sleep_until_tick;
};

StStatus StThread_Init(struct StThread **main_thread __out);

void StThread_EnablePreemption(void);
void StThread_DisablePreemption(void);
int StThread_IsPreemptionEnabled(void);

StStatus StThread_CreateKernel(
    StThread_EntryFunction entry __in,
    size_t stack_size __in,
    struct StThread **threadout __out
);
StStatus StThread_CreateUser(
    struct StProcess *proc __in,
    uintptr_t entry __in,
    size_t stack_size __in,
    uintptr_t ustack_top __in,
    struct StThread **threadout __out
);
StStatus StThread_Remove(struct StThread *thread __in);

StStatus StThread_Detach(struct StThread *thread __in);
StStatus StThread_Wait(
    struct StThread **list __in,
    int count __in,
    int timeout_ms __in
);

StStatus StThread_Sleep(int timeout_ms __in);

void StThread_Yield(void);

__noreturn
void StThread_Exit(void);

#endif // __STRATA_THREAD_H__
