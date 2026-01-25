#include <strata/process.h>

#include <stdlib.h>

#include <strata/thread.h>
#include <strata/types.h>

StStatus StProcess_CreateUser(
    struct StProcess **process __out, uintptr_t entry __in, uintptr_t stack_top __in
)
{
    static StProcess_Id new_process_id = (StProcess_Id)1;

    StStatus status;
    struct StProcess *proc = NULL;
    struct StMmuP_AddressSpace *asp = NULL;
    struct StThread *main_thread = NULL;

    proc = calloc(1, sizeof(*proc));
    if (!proc) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }

    proc->id = new_process_id++;

    // status = StMmuP_CreateAddressSpace(&asp);
    // if (!CHECK_SUCCESS(status)) goto has_error;

    // proc->address_space = asp;

    status = StThread_CreateUser(proc, entry, (St_PageCount)16, stack_top, &main_thread);
    if (!CHECK_SUCCESS(status)) goto has_error;

    proc->main_thread = main_thread;

    *process = proc;

    return STATUS_SUCCESS;

has_error:
    if (main_thread) {
        StThread_Remove(main_thread);
    }

    if (asp) {
        StMmuP_RemoveAddressSpace(asp);
    }

    if (proc) {
        free(proc);
    }

    return status;
}

void StProcess_Remove(struct StProcess *process)
{
    if (!process) return;

    StMmuP_RemoveAddressSpace(process->address_space);

    StThread_Remove(process->main_thread);

    free(process);
}
