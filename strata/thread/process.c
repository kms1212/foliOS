#include <strata/process.h>

#include <stdlib.h>

#include <strata/plat/thread.h>

#include <strata/mm.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/types.h>

StStatus StProcess_CreateUser(struct StProcess **process __out)
{
    static StProcess_Id new_process_id = (StProcess_Id)1;

    StStatus status;
    struct StProcess *proc = NULL;
    struct StMm_AddressSpace *asp = NULL;

    proc = calloc(1, sizeof(*proc));
    if (!proc) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    proc->id = new_process_id++;

    status = StMm_CreateAddressSpace(&asp);
    if (!CHECK_SUCCESS(status)) goto has_error;

    proc->address_space = asp;

    *process = proc;

    return STATUS_SUCCESS;

has_error:
    if (asp) {
        StMm_RemoveAddressSpace(asp);
    }

    if (proc) {
        free(proc);
    }

    return status;
}

void StProcess_Remove(struct StProcess *process)
{
    if (!process) return;

    process->is_dying = 1;

    StMm_RemoveAddressSpace(process->address_space);

    if (!process->main_thread->is_dying) {
        StThread_Remove(process->main_thread);
    }

    free(process);
}
