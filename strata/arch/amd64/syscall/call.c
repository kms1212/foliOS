#include <strata/syscall.h>

#include <inttypes.h>
#include <stdint.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/handle.h>
#include <strata/log.h>
#include <strata/module.h>
#include <strata/process.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>

#define MODULE_NAME "syscall"

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

static StStatus get_node_from_handle(uint32_t handle, struct StGnt_Node **node_out)
{
    StStatus status;
    struct StProcess *process;
    struct StGnt_Node *node;
    enum StHandle_Type type;

    status = get_current_process(&process);
    if (!CHECK_SUCCESS(status)) return status;

    status = StHandle_GetRetained(&process->handle_table, handle, &type, (void **)&node);
    if (!CHECK_SUCCESS(status)) return status;
    if (type != ST_HANDLE_TYPE_GNT_NODE) {
        StHandle_ReleaseObject(type, node);
        return STATUS_INVALID_HANDLE;
    }

    if (node_out) *node_out = node;

    return STATUS_SUCCESS;
}

static void build_reg_args(
    unsigned long arg0, unsigned long arg1, unsigned long arg2, unsigned long arg3, long args_out[4]
)
{
    args_out[0] = (long)arg0;
    args_out[1] = (long)arg1;
    args_out[2] = (long)arg2;
    args_out[3] = (long)arg3;
}

__weak StStatus StSyscallA_DispatchCallReg(
    struct StGnt_Node *node,
    uint32_t funcid,
    unsigned long arg0,
    unsigned long arg1,
    unsigned long arg2,
    unsigned long arg3,
    int *handled_out
)
{
    (void)node;
    (void)funcid;
    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;

    if (handled_out) *handled_out = 0;

    return STATUS_NOT_SUPPORTED;
}

__weak StStatus StSyscallA_DispatchCallPtr(
    struct StGnt_Node *node,
    uint32_t funcid,
    const void *args,
    void *result,
    unsigned long arg0,
    unsigned long arg1,
    int *handled_out
)
{
    (void)node;
    (void)funcid;
    (void)args;
    (void)result;
    (void)arg0;
    (void)arg1;

    if (handled_out) *handled_out = 0;

    return STATUS_NOT_SUPPORTED;
}

StStatus StSyscall_CallReg(
    uint32_t handle __in,
    uint32_t funcid __in,
    unsigned long arg0 __in,
    unsigned long arg1 __in,
    unsigned long arg2 __in,
    unsigned long arg3 __in
)
{
    StStatus status;
    struct StGnt_Node *node;
    struct StModule *handler_module;
    int handled = 0;
    long args[4];

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "call_reg: handle %" PRIu32 ", funcid %" PRIu32 "\n",
        handle,
        funcid
    );

    status = get_node_from_handle(handle, &node);
    if (!CHECK_SUCCESS(status)) return status;

    status = StSyscallA_DispatchCallReg(node, funcid, arg0, arg1, arg2, arg3, &handled);
    if (handled) {
        StGnt_ReleaseNode(node);
        return status;
    }

    build_reg_args(arg0, arg1, arg2, arg3, args);

    handler_module = node->handler_module;
    if (!handler_module || !handler_module->dispatch_args) {
        StGnt_ReleaseNode(node);
        return STATUS_NOT_SUPPORTED;
    }

    status = handler_module->dispatch_args(node, handle, funcid, args);
    StGnt_ReleaseNode(node);

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "call_reg result: handle %" PRIu32 ", funcid %" PRIu32 " -> %08" PRIX32 "\n",
        handle,
        funcid,
        status
    );

    return status;
}

StStatus StSyscall_CallPtr(
    uint32_t handle __in,
    uint32_t funcid __in,
    const void *args __in,
    void *result __out_optional,
    unsigned long arg0 __in,
    unsigned long arg1 __in
)
{
    StStatus status;
    struct StGnt_Node *node;
    struct StModule *handler_module;
    int handled = 0;
    long dispatch_args[4];

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "call_ptr: handle %" PRIu32 ", funcid %" PRIu32 "\n",
        handle,
        funcid
    );

    status = get_node_from_handle(handle, &node);
    if (!CHECK_SUCCESS(status)) return status;

    status = StSyscallA_DispatchCallPtr(node, funcid, args, result, arg0, arg1, &handled);
    if (handled) {
        StGnt_ReleaseNode(node);
        return status;
    }

    dispatch_args[0] = (long)(uintptr_t)args;
    dispatch_args[1] = (long)(uintptr_t)result;
    dispatch_args[2] = (long)arg0;
    dispatch_args[3] = (long)arg1;

    handler_module = node->handler_module;
    if (!handler_module || !handler_module->dispatch_args) {
        StGnt_ReleaseNode(node);
        return STATUS_NOT_SUPPORTED;
    }

    status = handler_module->dispatch_args(node, handle, funcid, dispatch_args);
    StGnt_ReleaseNode(node);

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "call_ptr result: handle %" PRIu32 ", funcid %" PRIu32 " -> %08" PRIX32 "\n",
        handle,
        funcid,
        status
    );

    return status;
}
