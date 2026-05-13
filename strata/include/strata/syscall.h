#ifndef __STRATA_SYSCALL_H__
#define __STRATA_SYSCALL_H__

#include <strata/plat/syscall.h>

#include <strata/gnt.h>
#include <strata/status.h>
#include <strata/uuid.h>

StStatus StSyscall_Open(const uint8_t *path __in, uint32_t flags __in, uint32_t *handle __out);
StStatus StSyscall_Close(uint32_t handle __in);
StStatus StSyscall_Query(
    uint32_t handle __in,
    const struct StUuid *if_uuid __in,
    uint32_t request_abiver __in,
    uint32_t *funcid_base __out,
    uint32_t *result_abiver __out
);
StStatus StSyscall_CallReg(
    uint32_t handle __in,
    uint32_t funcid __in,
    unsigned long arg0 __in,
    unsigned long arg1 __in,
    unsigned long arg2 __in,
    unsigned long arg3 __in
);
StStatus StSyscall_CallPtr(
    uint32_t handle __in,
    uint32_t funcid __in,
    const void *args __in,
    void *result __out_optional,
    unsigned long arg0 __in,
    unsigned long arg1 __in
);

StStatus StSyscallA_DispatchCallReg(
    StGnt_Node_StrongRef node __in,
    uint32_t funcid __in,
    unsigned long arg0 __in,
    unsigned long arg1 __in,
    unsigned long arg2 __in,
    unsigned long arg3 __in,
    int *handled_out __out
);
StStatus StSyscallA_DispatchCallPtr(
    StGnt_Node_StrongRef node __in,
    uint32_t funcid __in,
    const void *args __in,
    void *result __out_optional,
    unsigned long arg0 __in,
    unsigned long arg1 __in,
    int *handled_out __out
);

#endif  // __STRATA_SYSCALL_H__
