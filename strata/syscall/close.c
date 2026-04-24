#include <strata/syscall.h>

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/handle.h>
#include <strata/process.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>

static StStatus get_current_process(struct StProcess **process_out)
{
    StStatus status;
    struct StThread *thread;

    status = StScheduler_GetCurrentThread(&thread);
    if (!CHECK_SUCCESS(status)) return status;
    if (!thread || !thread->process) return STATUS_INVALID_THREAD;

    if (process_out) *process_out = thread->process;

    return STATUS_SUCCESS;
}

StStatus StSyscall_Close(uint32_t handle __in)
{
    StStatus status;
    struct StProcess *process;

    status = get_current_process(&process);
    if (!CHECK_SUCCESS(status)) return status;

    return StHandle_Close(&process->handle_table, handle);
}
