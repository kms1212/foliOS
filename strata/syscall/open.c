#include <strata/syscall.h>

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/handle.h>
#include <strata/status.h>

StStatus StSyscall_Open(const uint8_t *path __in, uint32_t flags __in, uint32_t *handle __out)
{
    StStatus status;
    StHandle new_handle;

    status = StHandle_Open(path, flags, &new_handle);
    if (!CHECK_SUCCESS(status)) return status;

    if (handle) *handle = (uint32_t)new_handle;

    return STATUS_SUCCESS;
}
