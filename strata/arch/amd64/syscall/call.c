#include <strata/syscall.h>

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/handle.h>
#include <strata/status.h>

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
    return StHandle_CallReg((StHandle)handle, funcid, arg0, arg1, arg2, arg3);
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
    return StHandle_CallPtr((StHandle)handle, funcid, args, result, arg0, arg1);
}
