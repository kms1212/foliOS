#include <strata/syscall.h>

#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/handle.h>
#include <strata/limits.h>
#include <strata/log.h>
#include <strata/process.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/utf.h>

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

StStatus StSyscall_Open(const uint8_t *path __in, uint32_t flags __in, uint32_t *handle __out)
{
    size_t path_len = strnlen((const char *)path, PATH_UTF8_MAX);
    St_Utf32Char path_buf[PATH_MAX];
    StStatus status;
    struct StGnt_Node *node;
    struct StProcess *process;
    uint32_t new_handle;

    status = StUtf_Utf8ToUtf32(path, path_len, path_buf, sizeof(path_buf), NULL);
    if (!CHECK_SUCCESS(status)) return status;

    status = StGnt_ResolvePath(g_gnt_root_local, path_buf, &node);
    if (!CHECK_SUCCESS(status)) return status;

    status = get_current_process(&process);
    if (!CHECK_SUCCESS(status)) return status;

    status = StHandle_Create(&process->handle_table, ST_HANDLE_TYPE_GNT_NODE, node, &new_handle);
    if (!CHECK_SUCCESS(status)) return status;

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "path: %s, flags: %08" PRIX32 " -> handle %" PRIu32 "\n",
        (const char *)path,
        flags,
        new_handle
    );

    if (handle) *handle = new_handle;

    return STATUS_SUCCESS;
}
