#include <strata/syscall.h>

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/handle.h>
#include <strata/uuid.h>

StStatus StSyscall_Query(
    uint32_t handle __in,
    const struct StUuid *if_uuid __in,
    uint32_t request_abiver __in,
    uint32_t *funcid_base __out,
    uint32_t *result_abiver __out
)
{
    return StHandle_Query((StHandle)handle, if_uuid, request_abiver, funcid_base, result_abiver);
}
