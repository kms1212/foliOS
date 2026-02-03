#include "strata/plat/thread.h"
#include <strata/process.h>

#include <stdlib.h>

#include <strata/thread.h>
#include <strata/types.h>

StStatus StProcess_CreateUser(
    struct StProcess **process __out,
    uintptr_t entry __in,
    int arg_count __in,
    const char *const *args __in,
    int env_count __in,
    const char *const *envs __in
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

    status = StThread_CreateUserMain(
        proc,
        entry,
        (St_PageCount)16,
        (St_PageCount)16,
        arg_count,
        args,
        env_count,
        envs,
        &main_thread
    );
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

    process->is_dying = 1;

    // StMmuP_RemoveAddressSpace(process->address_space);

    if (!process->main_thread->is_dying) {
        StThread_Remove(process->main_thread);
    }

    free(process);
}
