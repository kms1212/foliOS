#include <strata/syscall.h>

#include <inttypes.h>

#include <strata/log.h>
#include <strata/status.h>

#define MODULE_NAME "syscall"

StStatus StSyscall_Open(const uint8_t *path __in, uint32_t flags __in, uint32_t *handle __out)
{
    static uint32_t new_handle = 0;

    LOG_DEBUG(
        LM_CAT_UNCLASSIFIED,
        "path: %s, flags: %08" PRIX32 " -> handle %" PRId32 "\n",
        (const char *)path,
        flags,
        new_handle
    );

    if (handle) *handle = new_handle++;

    return STATUS_SUCCESS;
}
