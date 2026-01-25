#ifndef __STRATA_PROCESS_H__
#define __STRATA_PROCESS_H__

#include <strata/plat/mmu.h>
#include <strata/plat/process.h>

#include <strata/status.h>

enum StProcess_Type {
    PROCESS_TYPE_USER = 0,
    PROCESS_TYPE_MODULE,
};

typedef int StProcess_Id __nocast;

struct StProcess {
    struct StProcess *next;

    StProcess_Id id;
    enum StProcess_Type type;

    struct StProcessP_PlatformData platform_data;
    struct StMmuP_AddressSpace *address_space;

    struct StThread *main_thread;
    struct StThread *thread_list_head;
    struct StThread *thread_list_tail;
};

StStatus StProcess_CreateUser(
    struct StProcess **process __out, uintptr_t entry __in, uintptr_t stack_top __in
);
StStatus StProcess_CreateModule(struct StProcess **process __out);
void StProcess_Remove(struct StProcess *process __in);

#endif  // __STRATA_PROCESS_H__
