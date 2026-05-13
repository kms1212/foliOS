#ifndef __STRATA_PROCESS_H__
#define __STRATA_PROCESS_H__

#include <strata/handle.h>
#include <strata/plat/process.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/mm/owner.h>
#include <strata/ref_control.h>
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

struct StMm_AddressSpace;
struct StModule;
struct StThread;

#ifndef __STRATA_PROCESS_REFS_DEFINED__
#    define __STRATA_PROCESS_REFS_DEFINED__
struct StProcess;
typedef struct StProcess *StProcess_StrongRef __ref_strong;
typedef struct StProcess *StProcess_WeakRef __ref_weak;
typedef struct StProcess *StProcess_BorrowedRef __ref_borrowed;
typedef struct StProcess *StProcess_InternalRef __ref_internal;
#endif

#ifndef __STRATA_MM_ADDRESS_SPACE_REFS_DEFINED__
#    define __STRATA_MM_ADDRESS_SPACE_REFS_DEFINED__
typedef struct StMm_AddressSpace *StMm_AddressSpace_StrongRef __ref_strong;
typedef struct StMm_AddressSpace *StMm_AddressSpace_WeakRef __ref_weak;
typedef struct StMm_AddressSpace *StMm_AddressSpace_BorrowedRef __ref_borrowed;
typedef struct StMm_AddressSpace *StMm_AddressSpace_InternalRef __ref_internal;
#endif

#ifndef __STRATA_THREAD_REFS_DEFINED__
#    define __STRATA_THREAD_REFS_DEFINED__
typedef struct StThread *StThread_StrongRef __ref_strong;
typedef struct StThread *StThread_WeakRef __ref_weak;
typedef struct StThread *StThread_BorrowedRef __ref_borrowed;
typedef struct StThread *StThread_InternalRef __ref_internal;
#endif

struct StProcess {
    struct StRefControlBlock ref_control;

    StProcess_InternalRef next;

    StProcess_Id id;
    enum StProcess_Type type;
    enum StProcess_State state;

    StGnt_Node_StrongRef gnt_node;

    struct StProcessP_PlatformData platform_data;
    StMm_AddressSpace_StrongRef address_space;

    StThread_InternalRef main_thread;
    StThread_InternalRef thread_list_head;
    StThread_InternalRef thread_list_tail;

    struct StHandle_Table handle_table;

    struct StModule *module;

    uintptr_t tls_image_addr;
    size_t tls_file_size;
    size_t tls_mem_size;
    size_t tls_align;

    struct StMm_AllocationOwner alloc_owner;
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
