#include "internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/gnt/interface.h>
#include <strata/gnt/path.h>
#include <strata/gnt_refs.h>
#include <strata/limits.h>
#include <strata/log.h>
#include <strata/process.h>
#include <strata/process_refs.h>
#include <strata/ref_control.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/thread_refs.h>
#include <strata/utf.h>

#define MODULE_NAME "process"

static StStatus register_process_directory_interfaces(StGnt_Node_StrongRef node)
{
    StStatus status;

    status = StGnt_RegisterInterface(node, &StGntIf_Uuid_Process, 0, 21);
    if (!CHECK_SUCCESS(status)) return status;

    status = StGnt_RegisterInterface(node, &StGntIf_Uuid_Directory, 0, 11);
    if (!CHECK_SUCCESS(status)) return status;

    return StGnt_RegisterInterface(node, &StGntIf_Uuid_FileInfo, 0, 2);
}

static StStatus register_threads_directory_interfaces(StGnt_Node_StrongRef node)
{
    StStatus status;

    status = StGnt_RegisterInterface(node, &StGntIf_Uuid_Directory, 0, 11);
    if (!CHECK_SUCCESS(status)) return status;

    return StGnt_RegisterInterface(node, &StGntIf_Uuid_FileInfo, 0, 2);
}

static StStatus register_stdio_interfaces(StGnt_Node_StrongRef node)
{
    StStatus status;

    status = StGnt_RegisterInterface(node, &StGntIf_Uuid_ByteStream, 0, 6);
    if (!CHECK_SUCCESS(status)) return status;

    return StGnt_RegisterInterface(node, &StGntIf_Uuid_FileInfo, 0, 2);
}

static StStatus register_thread_interfaces(StGnt_Node_StrongRef node)
{
    StStatus status;

    status = StGnt_RegisterInterface(node, &StGntIf_Uuid_Thread, 0, 8);
    if (!CHECK_SUCCESS(status)) return status;

    return StGnt_RegisterInterface(node, &StGntIf_Uuid_FileInfo, 0, 2);
}

static StStatus get_current_process(struct StProcess **process_out)
{
    StStatus status;
    StThread_InternalRef thread;

    status = StScheduler_GetCurrentThread(&thread);
    if (!CHECK_SUCCESS(status)) return status;
    if (!thread || !thread->process) return STATUS_INVALID_THREAD;

    if (process_out) *process_out = thread->process;

    return STATUS_SUCCESS;
}

static StStatus format_id_name(
    int id, St_Utf32Char *name_out, size_t name_out_size, size_t *name_len_out
)
{
    char name_utf8[32];

    snprintf(name_utf8, sizeof(name_utf8), "%d", id);

    return StUtf_Utf8ToUtf32(
        (const St_Utf8Char *)name_utf8,
        strnlen(name_utf8, sizeof(name_utf8)),
        name_out,
        name_out_size,
        name_len_out
    );
}

static StGnt_Node_InternalRef find_registered_child(
    StGnt_Node_StrongRef parent, const St_Utf32Char *name, size_t name_len
)
{
    StGnt_Node_InternalRef child;

    if (!parent || parent->type == GNT_NODETYPE_LINK) return NULL;

    child = parent->children_head;
    while (child) {
        if (child->name_len == name_len &&
            memcmp(child->name, name, name_len * sizeof(*name)) == 0) {
            return child;
        }
        child = child->sibling;
    }

    return NULL;
}

static StStatus parse_decimal_id(const St_Utf32Char *token, size_t token_len, int *value_out)
{
    int value = 0;

    if (!token_len) return STATUS_INVALID_VALUE;

    for (size_t i = 0; i < token_len; i++) {
        if (token[i] < U'0' || token[i] > U'9') return STATUS_INVALID_VALUE;
        value = (value * 10) + (int)(token[i] - U'0');
    }

    if (value_out) *value_out = value;

    return STATUS_SUCCESS;
}

static StStatus get_process_id_from_process_node(
    StGnt_Node_StrongRef node, StProcess_Id *process_id_out
)
{
    if (!node) return STATUS_INVALID_VALUE;

    return parse_decimal_id(node->name, node->name_len, process_id_out);
}

static StStatus get_process_from_process_node(
    StGnt_Node_StrongRef node, StProcess_BorrowedRef *process_out
)
{
    StStatus status;
    StProcess_Id process_id;
    StProcess_BorrowedRef process;

    status = get_process_id_from_process_node(node, &process_id);
    if (!CHECK_SUCCESS(status)) return status;

    process = StProcess_FindById(process_id);
    if (!process) return STATUS_ENTRY_NOT_FOUND;

    if (process_out) *process_out = process;

    return STATUS_SUCCESS;
}

static StStatus register_process_node(StProcess_BorrowedRef process, StGnt_Node_StrongRef *node_out)
{
    StStatus status;
    St_Utf32Char process_name[NODENAME_MAX];
    size_t process_name_len;
    StGnt_Node_StrongRef node;

    if (!process) return STATUS_INVALID_VALUE;

    if (process->gnt_node) {
        status = register_process_directory_interfaces(process->gnt_node);
        if (!CHECK_SUCCESS(status)) return status;

        if (node_out) *node_out = process->gnt_node;
        return STATUS_SUCCESS;
    }

    status = format_id_name(process->id, process_name, sizeof(process_name), &process_name_len);
    if (!CHECK_SUCCESS(status)) return status;

    node = (StGnt_Node_StrongRef)
        find_registered_child(g_gnt_system_processes, process_name, process_name_len);
    if (node) {
        node->type = GNT_NODETYPE_DIRECTORY;
        node->handler_module = StProcess_Module;

        status = register_process_directory_interfaces(node);
        if (!CHECK_SUCCESS(status)) return status;

        process->gnt_node = node;
        if (node_out) *node_out = node;
        return STATUS_SUCCESS;
    }

    status = StGnt_AddNode(g_gnt_system_processes, process_name, &node);
    if (!CHECK_SUCCESS(status)) return status;

    node->type = GNT_NODETYPE_DIRECTORY;
    node->handler_module = StProcess_Module;

    status = register_process_directory_interfaces(node);
    if (!CHECK_SUCCESS(status)) return status;

    process->gnt_node = node;

    if (node_out) *node_out = node;

    return STATUS_SUCCESS;
}

static StStatus register_directory_child(
    StGnt_Node_StrongRef parent,
    const St_Utf32Char *name,
    size_t name_len,
    StGnt_Node_StrongRef *node_out
)
{
    StStatus status;
    StGnt_Node_StrongRef node;

    node = (StGnt_Node_StrongRef)find_registered_child(parent, name, name_len);
    if (node) {
        node->type = GNT_NODETYPE_DIRECTORY;
        node->handler_module = StProcess_Module;

        status = register_threads_directory_interfaces(node);
        if (!CHECK_SUCCESS(status)) return status;

        if (node_out) *node_out = node;
        return STATUS_SUCCESS;
    }

    status = StGnt_AddNode(parent, name, &node);
    if (!CHECK_SUCCESS(status)) return status;

    node->type = GNT_NODETYPE_DIRECTORY;
    node->handler_module = StProcess_Module;

    status = register_threads_directory_interfaces(node);
    if (!CHECK_SUCCESS(status)) return status;

    if (node_out) *node_out = node;

    return STATUS_SUCCESS;
}

static StStatus register_leaf_child(
    StGnt_Node_StrongRef parent,
    const St_Utf32Char *name,
    size_t name_len,
    StGnt_Node_StrongRef *node_out
)
{
    StStatus status;
    StGnt_Node_StrongRef node;
    int is_main_thread_leaf;

    is_main_thread_leaf = parent && parent->parent && parent->name_len == 7 &&
        StUtf_CompareUtf32Chars(parent->name, parent->name_len, U"Threads", 7) == 0 &&
        name_len == 4 && StUtf_CompareUtf32Chars(name, name_len, U"Main", 4) == 0;

    node = (StGnt_Node_StrongRef)find_registered_child(parent, name, name_len);
    if (!node) {
        status = StGnt_AddNode(parent, name, &node);
        if (!CHECK_SUCCESS(status)) return status;
    }

    node->type = GNT_NODETYPE_LEAF;
    node->handler_module = StProcess_Module;

    if (is_main_thread_leaf) {
        status = register_thread_interfaces(node);
    } else {
        status = register_stdio_interfaces(node);
    }
    if (!CHECK_SUCCESS(status)) return status;

    if (node_out) *node_out = node;

    return STATUS_SUCCESS;
}

static StStatus resolve_process_root(
    const St_Utf32Char *token, size_t token_len, StGnt_Node_StrongRef *next_node
)
{
    StStatus status;
    StProcess_BorrowedRef process;
    int process_id;

    if (StUtf_CompareUtf32Chars(token, token_len, U"Current", 7) == 0) {
        status = get_current_process(&process);
        if (!CHECK_SUCCESS(status)) return status;

        LOG_DEBUG(LM_CAT_UNCLASSIFIED, "Process GNT resolve Current\n");
        return register_process_node(process, next_node);
    }

    status = parse_decimal_id(token, token_len, &process_id);
    if (!CHECK_SUCCESS(status)) return STATUS_ENTRY_NOT_FOUND;

    process = StProcess_FindById(process_id);
    if (!process) return STATUS_ENTRY_NOT_FOUND;

    return register_process_node(process, next_node);
}

static StStatus resolve_process_directory(
    StGnt_Node_StrongRef base_node,
    const St_Utf32Char *token,
    size_t token_len,
    StGnt_Node_StrongRef *next_node
)
{
    StStatus status;
    StProcess_BorrowedRef process;

    status = get_process_from_process_node(base_node, &process);
    if (!CHECK_SUCCESS(status)) return status;

    if (StUtf_CompareUtf32Chars(token, token_len, U"Threads", 7) == 0) {
        return register_directory_child(base_node, U"Threads", 7, next_node);
    }

    if (StUtf_CompareUtf32Chars(token, token_len, U"Stdin", 5) == 0) {
        return register_leaf_child(base_node, U"Stdin", 5, next_node);
    }

    if (StUtf_CompareUtf32Chars(token, token_len, U"Stdout", 6) == 0) {
        return register_leaf_child(base_node, U"Stdout", 6, next_node);
    }

    if (StUtf_CompareUtf32Chars(token, token_len, U"Stderr", 6) == 0) {
        return register_leaf_child(base_node, U"Stderr", 6, next_node);
    }

    (void)process;
    return STATUS_ENTRY_NOT_FOUND;
}

static StStatus resolve_threads_directory(
    StGnt_Node_StrongRef base_node,
    const St_Utf32Char *token,
    size_t token_len,
    StGnt_Node_StrongRef *next_node
)
{
    StStatus status;
    StProcess_BorrowedRef process;

    if (!base_node || !base_node->parent) return STATUS_INVALID_VALUE;

    status = get_process_from_process_node((StGnt_Node_StrongRef)base_node->parent, &process);
    if (!CHECK_SUCCESS(status)) return status;

    if (StUtf_CompareUtf32Chars(token, token_len, U"Main", 4) == 0) {
        if (!process->main_thread ||
            StRefControlBlock_IsDying(&process->main_thread->ref_control)) {
            return STATUS_ENTRY_NOT_FOUND;
        }

        return register_leaf_child(base_node, U"Main", 4, next_node);
    }

    return STATUS_ENTRY_NOT_FOUND;
}

StStatus StProcessGnt_Resolve(
    StGnt_Node_StrongRef base_node __in,
    const St_Utf32Char *path __in,
    StGnt_Node_StrongRef *next_node __out,
    const St_Utf32Char **remaining_path __out
)
{
    assert(next_node);
    assert(remaining_path);

    StStatus status;
    St_Utf8Char path_utf8[512];
    size_t path_len;
    struct StGnt_PathCursor cursor;

    if (!base_node || !path) return STATUS_INVALID_VALUE;
    StGntPath_Begin(&cursor, path);
    if (StGntPath_Next(&cursor)) return STATUS_INVALID_VALUE;

    if (base_node == g_gnt_system_processes) {
        status = resolve_process_root(cursor.token, cursor.token_len, next_node);
    } else if (base_node->parent == g_gnt_system_processes) {
        status = resolve_process_directory(base_node, cursor.token, cursor.token_len, next_node);
    } else if (
        base_node->parent && base_node->name_len == 7 &&
        memcmp(base_node->name, U"Threads", 7 * sizeof(St_Utf32Char)) == 0
    ) {
        status = resolve_threads_directory(base_node, cursor.token, cursor.token_len, next_node);
    } else {
        status = STATUS_ENTRY_NOT_FOUND;
    }

    if (!CHECK_SUCCESS(status)) return status;

    path = StGntPath_Remaining(&cursor);

    status = StUtf_CountUtf32Chars(path, 256 * sizeof(*path), &path_len);
    if (!CHECK_SUCCESS(status)) return status;

    status = StUtf_Utf32ToUtf8(path, path_len, path_utf8, sizeof(path_utf8), NULL);
    if (!CHECK_SUCCESS(status)) return status;

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "Process GNT resolve %s\n", path_utf8);

    *remaining_path = path;

    return STATUS_SUCCESS;
}
