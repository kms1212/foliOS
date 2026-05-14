#include <strata/process.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>

#include <strata/handle.h>
#include <strata/plat/thread.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/log.h>
#include <strata/mm.h>
#include <strata/mm/asp.h>
#include <strata/mm/pool.h>
#include <strata/mm/types.h>
#include <strata/panic.h>
#include <strata/status.h>
#include <strata/thread.h>

#define MODULE_NAME                               "process"
#define PROCESS_CREATE_DEFERRED_REAP_BUDGET_PAGES ((St_PageCount)256)

static StProcess_InternalRef process_list_head = NULL;
static StProcess_InternalRef process_list_tail = NULL;
static atomic_uint_fast32_t process_count = 0;

static void finalize_process_object(void *object);

static void unlink_process(StProcess_InternalRef process)
{
    StProcess_InternalRef prev;

    if (!process) return;

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
}

StStatus StProcess_CreateUser(StProcess_StrongRef *process __out)
{
    assert(process);

    static StProcess_Id new_process_id = (StProcess_Id)1;

    StStatus status;
    StProcess_StrongRef proc = NULL;
    StMm_AddressSpace_StrongRef asp = NULL;
    uint32_t prev_process_count;

    StThread_RunDeferredReap(PROCESS_CREATE_DEFERRED_REAP_BUDGET_PAGES);

    status = StPool_AllocateClear(sizeof(*proc), (void **)&proc);
    if (!CHECK_SUCCESS(status)) goto has_error;

    proc->id = new_process_id++;
    StRefControlBlock_Init(&proc->ref_control, 1, proc, finalize_process_object);

    status = StMm_CreateAllocationOwner(&proc->alloc_owner);
    if (!CHECK_SUCCESS(status)) goto has_error;

    status = StMm_CreateAddressSpace(&asp, proc);
    if (!CHECK_SUCCESS(status)) goto has_error;

    proc->address_space = asp;
    proc->gnt_node = NULL;
    StHandle_TableInit(&proc->handle_table);
    proc->state = PROCESS_STATE_PENDING;
    proc->type = PROCESS_TYPE_USER;

    StThread_LockPreemption();

    if (!process_list_head) {
        process_list_head = process_list_tail = (StProcess_InternalRef)proc;
    } else {
        process_list_tail->next = (StProcess_InternalRef)proc;
        process_list_tail = (StProcess_InternalRef)proc;
    }

    prev_process_count = atomic_fetch_add(&process_count, 1);

    LOG_DEBUG(
        LM_CAT_UNCLASSIFIED,
        "created process #%d (active %d)\n",
        proc->id,
        prev_process_count + 1
    );

    StThread_UnlockPreemption();

    *process = proc;

    return STATUS_SUCCESS;

has_error:
    if (asp) {
        StMm_RemoveAddressSpace(asp);
    }

    if (proc) {
        if (proc->alloc_owner) {
            StMm_ReleaseAllocationOwner(proc->alloc_owner);
        }
        StPool_Free(proc);
    }

    return status;
}

void StProcess_BeginRemove(StProcess_StrongRef process)
{
    uint32_t prev_process_count;

    if (!process) return;

    StThread_LockPreemption();

    StRefControlBlock_MarkDying(&process->ref_control);
    unlink_process((StProcess_InternalRef)process);

    prev_process_count = atomic_fetch_sub(&process_count, 1);

    LOG_DEBUG(
        LM_CAT_UNCLASSIFIED,
        "removed process #%d (active %d)\n",
        process->id,
        prev_process_count - 1
    );

    StThread_UnlockPreemption();

    if (process->gnt_node) {
        StGnt_RemoveNode(process->gnt_node);
        process->gnt_node = NULL;
    }
}

void StProcess_Acquire(StProcess_StrongRef process __inout)
{
    assert(process);

    StRefControlBlock_Acquire(&process->ref_control);
}

void StProcess_Release(StProcess_StrongRef process __inout)
{
    assert(process);

    (void)StRefControlBlock_Release(&process->ref_control);
}

void StProcess_FinalizeRemove(StProcess_StrongRef process)
{
    StThread_StrongRef main_thread = NULL;
    StStatus status;

    if (!process) return;

    StHandle_TableClear(&process->handle_table);

    if (process->alloc_owner) {
        StMm_CloseAllocationOwner(process->alloc_owner);
    }

    StMm_RemoveAddressSpace(process->address_space);
    process->address_space = NULL;

    if (process->alloc_owner) {
        LOG_DEBUG(
            LM_CAT_UNCLASSIFIED,
            "leaked %zd pages\n",
            process->alloc_owner->page_usage_count
        );
        StMm_ReleaseAllocationOwner(process->alloc_owner);
        process->alloc_owner = NULL;
    }

    status = StThread_AcquireInternal(process->main_thread, &main_thread);
    if (CHECK_SUCCESS(status)) {
        status = StThread_Remove(main_thread);
        StThread_Release(main_thread);
        if (!CHECK_SUCCESS(status) && status != STATUS_THREAD_NOT_FINISHED) {
            St_Panic(status, "failed to remove process main thread");
        }
    }

    StPool_Free(process);
}

static void finalize_process_object(void *object)
{
    StProcess_FinalizeRemove((StProcess_StrongRef)object);
}

void StProcess_Remove(StProcess_StrongRef process)
{
    StThread_StrongRef main_thread = NULL;
    StStatus status;

    if (!process) return;

    StProcess_BeginRemove(process);

    status = StThread_AcquireInternal(process->main_thread, &main_thread);
    if (CHECK_SUCCESS(status)) {
        status = StThread_Remove(main_thread);
        StThread_Release(main_thread);
        if (!CHECK_SUCCESS(status) && status != STATUS_THREAD_NOT_FINISHED) {
            St_Panic(status, "failed to remove process main thread");
        }
        return;
    }

    StProcess_Release(process);
}

void StProcess_GetCount(uint32_t *count __out)
{
    assert(count);

    *count = atomic_load(&process_count);
}

StProcess_BorrowedRef StProcess_GetListHead(void)
{
    return (StProcess_BorrowedRef)process_list_head;
}

StProcess_BorrowedRef StProcess_FindById(StProcess_Id id)
{
    StProcess_InternalRef current = process_list_head;

    while (current) {
        if (current->id == id && !StRefControlBlock_IsDying(&current->ref_control)) {
            return (StProcess_BorrowedRef)current;
        }

        current = current->next;
    }

    return NULL;
}
