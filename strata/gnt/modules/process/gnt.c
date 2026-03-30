#include <stdio.h>

#include <strata/scheduler.h>
#include <string.h>

#include <strata/gnt.h>
#include <strata/log.h>
#include <strata/module.h>
#include <strata/panic.h>

#define MODULE_NAME "process"

struct StModule *StProcess_Module;

static int advance_token(
    const St_Utf32Char *path __in, const St_Utf32Char **next_token __out, size_t *next_len __out
)
{
    while (*path == '/') {
        path++;
    }
    if (*path == '\0') return 1;

    const St_Utf32Char *token_start = path;
    size_t len = 0;

    while (*path != '/' && *path != '\0') {
        if (len < NODENAME_MAX) {
            len++;
        }
        path++;
    }

    if (next_token) *next_token = token_start;
    if (next_len) *next_len = len;

    return 0;
}

static StStatus get_current_process_node(struct StGnt_Node **node __out)
{
    StStatus status;
    struct StThread *thread;
    struct StProcess *process;
    struct StGnt_Node *process_node;

    status = StScheduler_GetCurrentThread(&thread);
    if (!CHECK_SUCCESS(status)) return status;

    process = thread->process;
    process_node = process->gnt_node;

    if (node) *node = process_node;

    return STATUS_SUCCESS;
}

static StStatus gnt_resolve(
    struct StGnt_Node *base_node __in,
    const St_Utf32Char *path __in,
    struct StGnt_Node **next_node __out,
    const St_Utf32Char **remaining_path __out
)
{
    St_Utf8Char path_utf8[512];
    StStatus status;

    const St_Utf32Char *token;
    size_t token_len;
    struct StGnt_Node *current_node = base_node;

    if (advance_token(path, &token, &token_len)) return STATUS_INVALID_VALUE;

    if (StUtf_CompareUtf32Chars(token, token_len, U"Current", 7) == 0) {
        status = get_current_process_node(&current_node);
        if (!CHECK_SUCCESS(status)) return status;

        path += token_len;

        LOG_DEBUG(LM_CAT_UNCLASSIFIED, "Process GNT resolve Current\n");
    }

    size_t path_len;

    status = StUtf_CountUtf32Chars(path, 256 * sizeof(*path), &path_len);
    if (!CHECK_SUCCESS(status)) return status;

    status = StUtf_Utf32ToUtf8(path, path_len, path_utf8, sizeof(path_utf8), NULL);
    if (!CHECK_SUCCESS(status)) return status;

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "Process GNT resolve %s\n", path_utf8);

    return STATUS_NOT_IMPLEMENTED;
}

__constructor static void init_process_module(void)
{
    StStatus status;

    status = StModule_Create(&StProcess_Module);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "Failed to create process module");
    }

    StProcess_Module->resolve = gnt_resolve;
}
