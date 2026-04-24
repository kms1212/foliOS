#include <strata/process.h>

#include <stdlib.h>

#include <strata/handle.h>
#include <strata/plat/thread.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/log.h>
#include <strata/mm.h>
#include <strata/mm/asp.h>
#include <strata/mm/pool.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/types.h>

#define MODULE_NAME "process"

static struct StProcess *process_list_head = NULL;
static struct StProcess *process_list_tail = NULL;

StStatus StProcess_CreateUser(struct StProcess **process __out)
{
    static StProcess_Id new_process_id = (StProcess_Id)1;

    StStatus status;
    struct StProcess *proc = NULL;
    struct StMm_AddressSpace *asp = NULL;

    status = StPool_AllocateClear(sizeof(*proc), (void **)&proc);
    if (!CHECK_SUCCESS(status)) goto has_error;

    proc->id = new_process_id++;

    status = StMm_CreateAddressSpace(&asp, proc);
    if (!CHECK_SUCCESS(status)) goto has_error;

    proc->address_space = asp;
    proc->gnt_node = NULL;
    StHandle_InitTable(&proc->handle_table);
    proc->state = PROCESS_STATE_PENDING;
    proc->type = PROCESS_TYPE_USER;

    StThread_LockPreemption();
    if (!process_list_head) {
        process_list_head = process_list_tail = proc;
    } else {
        process_list_tail->next = proc;
        process_list_tail = proc;
    }
    StThread_UnlockPreemption();

    *process = proc;

    return STATUS_SUCCESS;

has_error:
    if (asp) {
        StMm_RemoveAddressSpace(asp);
    }

    if (proc) {
        StPool_Free(proc);
    }

    return status;
}

void StProcess_Remove(struct StProcess *process)
{
    struct StProcess *prev;

    if (!process) return;

    StThread_LockPreemption();

    process->is_dying = 1;

    prev = NULL;
    if (process_list_head == process) {
        process_list_head = process->next;
    } else {
        prev = process_list_head;
        while (prev && prev->next != process) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = process->next;
        }
    }

    if (process_list_tail == process) {
        process_list_tail = prev;
    }

    process->next = NULL;

    StThread_UnlockPreemption();

    StHandle_ClearTable(&process->handle_table);

    if (process->gnt_node) {
        StGnt_RemoveNode(process->gnt_node);
        process->gnt_node = NULL;
    }

    StMm_CleanupOwnerAllocation(&process->alloc_owner);

    StMm_RemoveAddressSpace(process->address_space);

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "leaked %zd pages\n", process->alloc_owner.page_usage_count);

    if (process->main_thread && !process->main_thread->is_dying) {
        StThread_Remove(process->main_thread);
    }

    StPool_Free(process);
}

struct StProcess *StProcess_GetListHead(void)
{
    return process_list_head;
}

struct StProcess *StProcess_FindById(StProcess_Id id)
{
    struct StProcess *current = process_list_head;

    while (current) {
        if (current->id == id && !current->is_dying) {
            return current;
        }

        current = current->next;
    }

    return NULL;
}
