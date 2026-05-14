#ifndef __STRATA_PROCESS_H__
#define __STRATA_PROCESS_H__

#include <strata/handle.h>
#include <strata/plat/process.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/mm/address_space_refs.h>
#include <strata/mm/allocation_owner_refs.h>
#include <strata/process_refs.h>
#include <strata/ref_control.h>
#include <strata/status.h>
#include <strata/thread_refs.h>

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
    struct StRefControlBlock ref_control;

    StProcess_InternalRef next;

    StProcess_Id id;
    enum StProcess_Type type;
    enum StProcess_State state;

    StGnt_Node_StrongRef gnt_node;

    struct StProcessP_PlatformData platform_data;
    StAddressSpace_StrongRef address_space;

    StThread_InternalRef main_thread;
    StThread_InternalRef thread_list_head;
    StThread_InternalRef thread_list_tail;

    struct StHandle_Table handle_table;

    struct StModule *module;

    uintptr_t tls_image_addr;
    size_t tls_file_size;
    size_t tls_mem_size;
    size_t tls_align;

    StAllocationOwner_StrongRef alloc_owner;
};

extern struct StModule *StProcess_Module;

StStatus StProcess_CreateUser(StProcess_StrongRef *process __out);
StStatus StProcess_CreateModule(StProcess_StrongRef *process __out);
void StProcess_Acquire(StProcess_StrongRef process __inout);
void StProcess_Release(StProcess_StrongRef process __inout);
void StProcess_BeginRemove(StProcess_StrongRef process __in);
void StProcess_FinalizeRemove(StProcess_StrongRef process __in);
void StProcess_Remove(StProcess_StrongRef process __in);
void StProcess_GetCount(uint32_t *count __out);
StProcess_BorrowedRef StProcess_GetListHead(void);
StProcess_BorrowedRef StProcess_FindById(StProcess_Id id);

#endif  // __STRATA_PROCESS_H__
