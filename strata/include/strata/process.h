#ifndef __STRATA_PROCESS_H__
#define __STRATA_PROCESS_H__

#include <strata/handle.h>
#include <strata/plat/process.h>

#include <strata/gnt.h>
#include <strata/mm/owner.h>
#include <strata/status.h>

enum StProcess_State {
    PROCESS_STATE_PENDING = 0,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_SUSPENDED,
    PROCESS_STATE_TERMINATED,
};

enum StProcess_Type {
    PROCESS_TYPE_USER = 0,
    PROCESS_TYPE_MODULE,
};

typedef int StProcess_Id __nocast;

struct StModule;

struct StProcess {
    struct StProcess *next;

    StProcess_Id id;
    enum StProcess_Type type;
    enum StProcess_State state;
    int is_dying;

    struct StGnt_Node *gnt_node;

    struct StProcessP_PlatformData platform_data;
    struct StMm_AddressSpace *address_space;

    struct StThread *main_thread;
    struct StThread *thread_list_head;
    struct StThread *thread_list_tail;

    struct StHandle_Table handle_table;

    struct StModule *module;

    struct StMm_AllocationOwner alloc_owner;
};

extern struct StModule *StProcess_Module;

StStatus StProcess_CreateUser(struct StProcess **process __out);
StStatus StProcess_CreateModule(struct StProcess **process __out);
void StProcess_Remove(struct StProcess *process __in);
struct StProcess *StProcess_GetListHead(void);
struct StProcess *StProcess_FindById(StProcess_Id id);

#endif  // __STRATA_PROCESS_H__
