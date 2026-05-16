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

/** Scheduler/lifetime-visible process state. */
enum StProcess_State {
    /** Process object exists but its main thread has not started running. */
    PROCESS_STATE_PENDING = 0,
    /** Process has runnable or running thread state. */
    PROCESS_STATE_RUNNING,
    /** Process is intentionally not scheduled. */
    PROCESS_STATE_SUSPENDED,
    /** Process has exited; final cleanup may still be pending. */
    PROCESS_STATE_TERMINATED,
};

/** Process execution class. */
enum StProcess_Type {
    PROCESS_TYPE_USER = 0,
    PROCESS_TYPE_MODULE,
};

typedef int StProcess_Id __nocast;

struct StModule;

/**
 * Ref-counted Strata process object.
 *
 * A process owns the local address space, handle table, allocation owner, and
 * intrusive thread list. Removal is phased so thread exit and process teardown
 * can coordinate without freeing objects behind outstanding strong references.
 */
struct StProcess {
    /** First-field ref control block used by StProcess_Acquire/Release. */
    struct StRefControlBlock ref_control;

    /** Global process-list link. */
    StProcess_InternalRef next;

    StProcess_Id id;
    enum StProcess_Type type;
    enum StProcess_State state;
    /** Kernel-level exit status; runtime-specific conversion happens in SDK. */
    StStatus exit_status;

    /** GNT node exposing process resources. */
    StGnt_Node_StrongRef gnt_node;

    struct StProcessP_PlatformData platform_data;
    /** Local user address space for the process. */
    StAddressSpace_StrongRef address_space;

    /** Main thread internal link; acquire before dereferencing across reap. */
    StThread_InternalRef main_thread;
    /** Process-owned intrusive thread list. */
    StThread_InternalRef thread_list_head;
    StThread_InternalRef thread_list_tail;

    /** Per-process handle table. */
    struct StHandle_Table handle_table;

    /** Backing module for module processes, NULL for ordinary user processes. */
    struct StModule *module;

    /** User image program header table virtual address for AT_PHDR. */
    uintptr_t program_header_addr;
    /** User image program header entry byte size for AT_PHENT. */
    size_t program_header_entry_size;
    /** User image program header count for AT_PHNUM. */
    unsigned int program_header_count;

    /** Memory accounting and cleanup owner for process allocations. */
    StAllocationOwner_StrongRef alloc_owner;
};

extern struct StModule *StProcess_Module;

/** Create a user process and return its initial strong reference. */
StStatus StProcess_CreateUser(StProcess_StrongRef *process __out);
/** Create a module process and return its initial strong reference. */
StStatus StProcess_CreateModule(StProcess_StrongRef *process __out);
/** Acquire another strong reference to a live process object. */
void StProcess_Acquire(StProcess_StrongRef process __inout);
/** Release a strong process reference. May finalize if this was the last ref. */
void StProcess_Release(StProcess_StrongRef process __inout);
/** Mark a process dying and remove it from public lookup/list state. */
void StProcess_BeginRemove(StProcess_StrongRef process __in);
/** Release process-owned subresources after public removal has begun. */
void StProcess_FinalizeRemove(StProcess_StrongRef process __in);
/** Begin and, when possible, finalize process removal. */
void StProcess_Remove(StProcess_StrongRef process __in);
/** Write the current live process count. */
void StProcess_GetCount(uint32_t *count __out);
/** Return the borrowed head of the process list. Caller must provide stability. */
StProcess_BorrowedRef StProcess_GetListHead(void);
/** Find a process by id and return a borrowed view if it is still visible. */
StProcess_BorrowedRef StProcess_FindById(StProcess_Id id __in);

#endif  // __STRATA_PROCESS_H__
