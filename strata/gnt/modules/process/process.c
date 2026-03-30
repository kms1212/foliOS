#include <strata/mm/pmm.h>
#include <strata/process.h>

#include <stdlib.h>

#include <strata/plat/thread.h>

#include <strata/log.h>
#include <strata/mm.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/types.h>

#define MODULE_NAME "process"

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
    if (!process) return;

    process->is_dying = 1;

    StMm_CleanupOwnerAllocation(&process->alloc_owner);

    StMm_RemoveAddressSpace(process->address_space);

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "leaked %zd pages\n", process->alloc_owner.page_usage_count);

    if (!process->main_thread->is_dying) {
        StThread_Remove(process->main_thread);
    }

    StPool_Free(process);
}
