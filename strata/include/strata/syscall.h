#ifndef __STRATA_SYSCALL_H__
#define __STRATA_SYSCALL_H__

#include <strata/plat/syscall.h>

#include <strata/status.h>
#include <strata/uuid.h>

StStatus StSyscall_Open(const uint8_t *path __in, uint32_t flags __in, uint32_t *handle __out);
StStatus StSyscall_Close(uint32_t handle __in);
StStatus StSyscall_Query(
    uint32_t handle __in,
    const struct StUuid *if_uuid __in,
    uint32_t request_groupid __in,
    uint32_t request_abiver __in,
    uint32_t *funcid_base __out,
    uint32_t *result_abiver __out
);

#endif  // __STRATA_SYSCALL_H__
