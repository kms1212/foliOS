#include <strata/syscall.h>

#include <inttypes.h>

#include <strata/gnt.h>
#include <strata/log.h>
#include <strata/status.h>
#include <strata/utf.h>

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

    size_t path_len = strnlen((const char *)path, PATH_UTF8_MAX);
    St_Utf32Char path_buf[PATH_MAX];
    StStatus status;

    status = StUtf_Utf8ToUtf32(path, path_len, path_buf, sizeof(path_buf), NULL);
    if (!CHECK_SUCCESS(status)) return status;

    status = StGnt_ResolvePath(g_gnt_root_local, path_buf, NULL);
    if (!CHECK_SUCCESS(status)) return status;

    if (handle) *handle = new_handle++;

    return STATUS_SUCCESS;
}
