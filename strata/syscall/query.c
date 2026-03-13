#include <strata/syscall.h>

#include <strata/status.h>
#include <strata/uuid.h>

StStatus StSyscall_Query(
    uint32_t handle __in,
    const struct StUuid *if_uuid __in,
    uint32_t request_groupid __in,
    uint32_t request_abiver __in,
    uint32_t *funcid_base __out,
    uint32_t *result_abiver __out
)
{
    return STATUS_NOT_IMPLEMENTED;
}
