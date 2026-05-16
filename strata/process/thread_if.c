#include "internal.h"

#include <stddef.h>
#include <stdint.h>

#include <strata/plat/thread.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/gnt_refs.h>
#include <strata/handle.h>
#include <strata/process.h>
#include <strata/process_refs.h>
#include <strata/ref_control.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/thread_refs.h>
#include <strata/utf.h>

#include "sidl/thread.server.h"
#include "sidl/thread.types.h"

#define MODULE_NAME "thread"

struct thread_dispatch_context {
    StThread_InternalRef thread;
};

static StStatus parse_decimal_id(const St_Utf32Char *token, size_t token_len, int *value_out)
{
    int value = 0;

    if (!token || !token_len) return STATUS_INVALID_VALUE;

    for (size_t i = 0; i < token_len; i++) {
        if (token[i] < U'0' || token[i] > U'9') return STATUS_INVALID_VALUE;
        value = (value * 10) + (int)(token[i] - U'0');
    }

    if (value_out) *value_out = value;

    return STATUS_SUCCESS;
}

static StStatus get_process_from_process_node(
    StGnt_Node_StrongRef process_node, StProcess_BorrowedRef *process_out
)
{
    StStatus status;
    int process_id;
    StProcess_BorrowedRef process;

    if (!process_node || process_node->parent != g_gnt_system_processes) {
        return STATUS_INVALID_HANDLE;
    }

    status = parse_decimal_id(process_node->name, process_node->name_len, &process_id);
    if (!CHECK_SUCCESS(status)) return status;

    process = StProcess_FindById(process_id);
    if (!process) return STATUS_ENTRY_NOT_FOUND;

    if (process_out) *process_out = process;

    return STATUS_SUCCESS;
}

static StStatus get_thread_from_thread_node(
    StGnt_Node_StrongRef thread_node, StThread_InternalRef *thread_out
)
{
    StStatus status;
    StProcess_BorrowedRef process;
    StThread_InternalRef thread;

    if (!thread_node || thread_node->name_len != 4 ||
        StUtf_CompareUtf32Chars(thread_node->name, thread_node->name_len, U"Main", 4) != 0) {
        return STATUS_INVALID_HANDLE;
    }

    if (!thread_node->parent || thread_node->parent->name_len != 7 ||
        StUtf_CompareUtf32Chars(
            thread_node->parent->name,
            thread_node->parent->name_len,
            U"Threads",
            7
        ) != 0) {
        return STATUS_INVALID_HANDLE;
    }

    if (!thread_node->parent->parent) {
        return STATUS_INVALID_HANDLE;
    }

    status =
        get_process_from_process_node((StGnt_Node_StrongRef)thread_node->parent->parent, &process);
    if (!CHECK_SUCCESS(status)) return status;

    thread = process->main_thread;
    if (!thread || StRefControlBlock_IsDying(&thread->ref_control)) return STATUS_ENTRY_NOT_FOUND;

    if (thread_out) *thread_out = thread;

    return STATUS_SUCCESS;
}

static StStatus thr_suspend(void *context, StHandle handle)
{
    (void)context;
    (void)handle;
    return STATUS_NOT_SUPPORTED;
}

static StStatus thr_resume(void *context, StHandle handle)
{
    (void)context;
    (void)handle;
    return STATUS_NOT_SUPPORTED;
}

static StStatus thr_get_state(void *context, StHandle handle, StIfThr_State *state)
{
    struct thread_dispatch_context *ctx = (struct thread_dispatch_context *)context;

    (void)handle;
    if (!ctx || !ctx->thread || !state) return STATUS_INVALID_VALUE;

    switch (ctx->thread->state) {
    case THREAD_STATE_PENDING:
        *state = THREAD_STATE_CREATED;
        return STATUS_SUCCESS;
    case THREAD_STATE_RUNNING:
        *state = THREAD_STATE_RUNNING;
        return STATUS_SUCCESS;
    case THREAD_STATE_FINISHED:
        *state = THREAD_STATE_TERMINATED;
        return STATUS_SUCCESS;
    default:
        *state = THREAD_STATE_SUSPENDED;
        return STATUS_SUCCESS;
    }
}

static StStatus thr_get_id(void *context, StHandle handle, uint64_t *tid)
{
    struct thread_dispatch_context *ctx = (struct thread_dispatch_context *)context;

    (void)handle;
    if (!ctx || !ctx->thread || !tid) return STATUS_INVALID_VALUE;

    *tid = (uint64_t)ctx->thread->id;
    return STATUS_SUCCESS;
}

static StStatus thr_terminate(void *context, StHandle handle, StStatus exit_code)
{
    (void)context;
    (void)handle;
    (void)exit_code;
    return STATUS_NOT_SUPPORTED;
}

static StStatus thr_set_tls_base(
    void *context, StHandle handle, StIfThr_RegisterId register_id, void *tls_address
)
{
    struct thread_dispatch_context *ctx = (struct thread_dispatch_context *)context;

    (void)handle;
    if (!ctx || !ctx->thread) return STATUS_INVALID_VALUE;

    switch (register_id) {
    case THREAD_REGISTERID_FS:
        return StThreadP_SetFsBase(ctx->thread, (uintptr_t)tls_address);
    case THREAD_REGISTERID_GS:
        return StThreadP_SetGsBase(ctx->thread, (uintptr_t)tls_address);
    default:
        return STATUS_INVALID_VALUE;
    }
}

static StStatus thr_get_tls_base(
    void *context, StHandle handle, StIfThr_RegisterId register_id, void **tls_address
)
{
    struct thread_dispatch_context *ctx = (struct thread_dispatch_context *)context;

    (void)handle;
    if (!ctx || !ctx->thread || !tls_address) return STATUS_INVALID_VALUE;

    switch (register_id) {
    case THREAD_REGISTERID_FS:
        *tls_address = (void *)ctx->thread->platform_data.fs_base;
        return STATUS_SUCCESS;
    case THREAD_REGISTERID_GS:
        *tls_address = (void *)ctx->thread->platform_data.gs_base;
        return STATUS_SUCCESS;
    default:
        return STATUS_INVALID_VALUE;
    }
}

static StStatus thr_wait(
    void *context,
    StHandle handle,
    StIfThr_WaitVector **vector,
    uint64_t vector_size,
    uint64_t timeout_ms
)
{
    (void)context;
    (void)handle;
    (void)vector;
    (void)vector_size;
    (void)timeout_ms;
    return STATUS_NOT_SUPPORTED;
}

static const StIfThr_ServerVTable g_thr_vtable = {
    .Suspend = thr_suspend,
    .Resume = thr_resume,
    .GetState = thr_get_state,
    .GetId = thr_get_id,
    .Terminate = thr_terminate,
    .SetTlsBase = thr_set_tls_base,
    .GetTlsBase = thr_get_tls_base,
    .Wait = thr_wait,
};

StStatus StThreadIf_DispatchCallArgs(
    StGnt_Node_StrongRef node __in,
    StHandle_Id handle __in,
    uint32_t funcid __in,
    const long args[4]
)
{
    StStatus status;
    struct thread_dispatch_context ctx;

    if (!node || !args) return STATUS_INVALID_VALUE;

    status = get_thread_from_thread_node(node, &ctx.thread);
    if (status == STATUS_INVALID_HANDLE) return STATUS_NOT_SUPPORTED;
    if (!CHECK_SUCCESS(status)) return status;

    return StIfThr_ServerDispatchArgs(&g_thr_vtable, &ctx, (StHandle)handle, funcid, args);
}
