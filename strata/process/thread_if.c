#include "internal.h"

#include <stddef.h>
#include <stdint.h>

#include <strata/plat/thread.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/handle.h>
#include <strata/mm/utils.h>
#include <strata/process.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/utf.h>

#include "sidl/thread.server.h"
#include "sidl/thread.types.h"

#define MODULE_NAME "thread"

struct thread_dispatch_context {
    StThread_InternalRef thread;
};

static int node_name_equals(
    const struct StGnt_Node *node, const St_Utf32Char *name, size_t name_len
)
{
    if (!node) return 0;
    if (node->name_len != name_len) return 0;

    return StUtf_CompareUtf32Chars(node->name, node->name_len, name, name_len) == 0;
}

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

static int is_process_node(const struct StGnt_Node *node)
{
    return node && node->parent == g_gnt_system_processes;
}

static int is_thread_node(const struct StGnt_Node *node)
{
    return node && node_name_equals(node, U"Main", 4) && node->parent &&
        node_name_equals(node->parent, U"Threads", 7) && node->parent->parent &&
        is_process_node(node->parent->parent);
}

static StStatus get_process_from_process_node(
    StGnt_Node_StrongRef process_node, StProcess_BorrowedRef *process_out
)
{
    StStatus status;
    int process_id;
    StProcess_BorrowedRef process;

    if (!is_process_node(process_node)) return STATUS_INVALID_HANDLE;

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

    if (!is_thread_node(thread_node)) return STATUS_INVALID_HANDLE;

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

static StStatus initialize_static_tls(StThread_InternalRef thread, uintptr_t fs_base)
{
    static const size_t k_copy_chunk_size = 256;

    StStatus status;
    StProcess_StrongRef process;
    uintptr_t tls_start;
    uint8_t buf[k_copy_chunk_size];
    size_t remaining;
    size_t copied;

    if (!thread || !thread->process) return STATUS_INVALID_VALUE;

    process = thread->process;
    if (!process->tls_mem_size) return STATUS_SUCCESS;
    if (!fs_base || process->tls_mem_size > fs_base) return STATUS_INVALID_VALUE;

    tls_start = fs_base - process->tls_mem_size;

    status = StMm_SetLocal(
        process->address_space,
        tls_start,
        0,
        process->tls_mem_size
    );
    if (!CHECK_SUCCESS(status)) return status;

    remaining = process->tls_file_size;
    copied = 0;
    while (remaining) {
        size_t chunk = remaining;
        if (chunk > sizeof(buf)) {
            chunk = sizeof(buf);
        }

        status = StMm_ReadLocal(
            process->address_space,
            process->tls_image_addr + copied,
            buf,
            chunk
        );
        if (!CHECK_SUCCESS(status)) return status;

        status = StMm_WriteLocal(process->address_space, tls_start + copied, buf, chunk);
        if (!CHECK_SUCCESS(status)) return status;

        copied += chunk;
        remaining -= chunk;
    }

    return STATUS_SUCCESS;
}

static StStatus thr_set_tls_base(
    void *context, StHandle handle, StIfThr_RegisterId register_id, void *tls_address
)
{
    struct thread_dispatch_context *ctx = (struct thread_dispatch_context *)context;
    StStatus status;

    (void)handle;
    if (!ctx || !ctx->thread) return STATUS_INVALID_VALUE;

    switch (register_id) {
    case THREAD_REGISTERID_FS:
        status = initialize_static_tls(ctx->thread, (uintptr_t)tls_address);
        if (!CHECK_SUCCESS(status)) return status;
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
    if (!is_thread_node(node)) return STATUS_NOT_SUPPORTED;

    status = get_thread_from_thread_node(node, &ctx.thread);
    if (!CHECK_SUCCESS(status)) return status;

    return StIfThr_ServerDispatchArgs(&g_thr_vtable, &ctx, (StHandle)handle, funcid, args);
}
