#include "internal.h"

#include <stddef.h>
#include <stdint.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/gnt_refs.h>
#include <strata/handle.h>
#include <strata/status.h>

#include "sidl/byte_stream.server.h"
#include "sidl/byte_stream.types.h"

enum stdio_node_kind {
    STDIO_NODE_STDIN,
    STDIO_NODE_STDOUT,
    STDIO_NODE_STDERR,
};

struct stdio_dispatch_context {
    enum stdio_node_kind kind;
};

extern int early_print_char(void *_state, char ch);
extern struct print_state pstate;

static void stdio_write(const uint8_t *buf, uint64_t size)
{
    for (uint64_t i = 0; i < size; i++) {
        early_print_char(&pstate, (char)buf[i]);
    }
}

static StStatus bs_seek(
    void *context, StHandle handle, int64_t offset, uint32_t whence, int64_t *result
)
{
    (void)context;
    (void)handle;
    (void)offset;
    (void)whence;

    if (result) *result = 0;
    return STATUS_NOT_SUPPORTED;
}

static StStatus bs_tell(void *context, StHandle handle, int64_t *offset)
{
    (void)context;
    (void)handle;
    if (!offset) return STATUS_INVALID_VALUE;

    *offset = 0;
    return STATUS_SUCCESS;
}

static StStatus bs_read(
    void *context,
    StHandle handle,
    uint8_t *buf,
    uint64_t size,
    StIfBs_IoFlags flags,
    uint64_t *result
)
{
    struct stdio_dispatch_context *ctx = (struct stdio_dispatch_context *)context;

    (void)handle;
    (void)buf;
    (void)size;
    (void)flags;

    if (!ctx || !result) return STATUS_INVALID_VALUE;

    *result = 0;
    if (ctx->kind != STDIO_NODE_STDIN) return STATUS_END_OF_FILE;

    return STATUS_SUCCESS;
}

static StStatus bs_write(
    void *context,
    StHandle handle,
    const uint8_t *buf,
    uint64_t size,
    StIfBs_IoFlags flags,
    uint64_t *result
)
{
    struct stdio_dispatch_context *ctx = (struct stdio_dispatch_context *)context;

    (void)handle;
    (void)flags;

    if (!ctx || !result) return STATUS_INVALID_VALUE;

    *result = 0;
    if (ctx->kind == STDIO_NODE_STDIN) return STATUS_NOT_SUPPORTED;
    if (size && !buf) return STATUS_INVALID_VALUE;

    if (size) {
        stdio_write(buf, size);
    }

    *result = size;
    return STATUS_SUCCESS;
}

static StStatus bs_sync(void *context, StHandle handle)
{
    (void)context;
    (void)handle;
    return STATUS_SUCCESS;
}

static StStatus bs_get_length(void *context, StHandle handle, uint64_t *length)
{
    (void)context;
    (void)handle;

    if (length) *length = 0;
    return STATUS_NOT_SUPPORTED;
}

static const StIfBs_ServerVTable g_bs_vtable = {
    .Seek = bs_seek,
    .Tell = bs_tell,
    .Read = bs_read,
    .Write = bs_write,
    .Sync = bs_sync,
    .GetLength = bs_get_length,
};

StStatus StStdioIf_DispatchCallArgs(
    StGnt_Node_StrongRef node __in,
    StHandle_Id handle __in,
    uint32_t funcid __in,
    const long args[4]
)
{
    StStatus status;
    struct StProcessGnt_NodeData *node_data;
    struct stdio_dispatch_context ctx;

    if (!node || !args) return STATUS_INVALID_VALUE;
    if (!node->parent) return STATUS_NOT_SUPPORTED;

    status = StProcessGnt_GetProcessFromNode((StGnt_Node_StrongRef)node->parent, NULL);
    if (status == STATUS_INVALID_HANDLE) return STATUS_NOT_SUPPORTED;
    if (!CHECK_SUCCESS(status)) return status;

    node_data = node->private_data;
    if (!node_data) return STATUS_NOT_SUPPORTED;

    switch (node_data->kind) {
    case PROCESS_GNT_NODE_STDIO_STDIN:
        ctx.kind = STDIO_NODE_STDIN;
        break;
    case PROCESS_GNT_NODE_STDIO_STDOUT:
        ctx.kind = STDIO_NODE_STDOUT;
        break;
    case PROCESS_GNT_NODE_STDIO_STDERR:
        ctx.kind = STDIO_NODE_STDERR;
        break;
    default:
        return STATUS_NOT_SUPPORTED;
    }

    return StIfBs_ServerDispatchArgs(&g_bs_vtable, &ctx, (StHandle)handle, funcid, args);
}
