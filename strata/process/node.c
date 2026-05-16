#include "internal.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/gnt_refs.h>
#include <strata/process.h>
#include <strata/process_refs.h>
#include <strata/ref_control.h>
#include <strata/status.h>
#include <strata/utf.h>

struct StProcessGnt_NodeData StProcessGnt_StdinNodeData = {
    .kind = PROCESS_GNT_NODE_STDIO_STDIN,
};

struct StProcessGnt_NodeData StProcessGnt_StdoutNodeData = {
    .kind = PROCESS_GNT_NODE_STDIO_STDOUT,
};

struct StProcessGnt_NodeData StProcessGnt_StderrNodeData = {
    .kind = PROCESS_GNT_NODE_STDIO_STDERR,
};

StStatus StProcessGnt_ParseProcessId(
    const St_Utf32Char *name __in, size_t name_len __in, StProcess_Id *process_id __out
)
{
    StProcess_Id value = 0;

    assert(process_id);

    if (!name || !name_len) return STATUS_INVALID_VALUE;

    for (size_t i = 0; i < name_len; i++) {
        StProcess_Id digit;

        if (name[i] < U'0' || name[i] > U'9') return STATUS_INVALID_VALUE;

        digit = (StProcess_Id)(name[i] - U'0');
        if (value > (INT_MAX - digit) / 10) return STATUS_INVALID_VALUE;

        value = (StProcess_Id)((value * 10) + digit);
    }

    *process_id = value;

    return STATUS_SUCCESS;
}

int StProcessGnt_IsProcessRootNode(StGnt_Node_InternalRef node __in)
{
    if (!node || !node->parent) return 0;
    if (node->parent->parent != (StGnt_Node_InternalRef)g_gnt_root_local) return 0;
    if (node->handler_module != StProcess_Module) return 0;
    if (node->parent->name_len != 6 ||
        memcmp(node->parent->name, U"System", 6 * sizeof(St_Utf32Char)) != 0) {
        return 0;
    }

    return node->name_len == 9 && memcmp(node->name, U"Processes", 9 * sizeof(St_Utf32Char)) == 0;
}

StStatus StProcessGnt_GetProcessFromNode(
    StGnt_Node_StrongRef process_node __in, StProcess_BorrowedRef *process_out __out_optional
)
{
    StProcess_InternalRef process;

    if (!process_node || !StProcessGnt_IsProcessRootNode(process_node->parent)) {
        return STATUS_INVALID_HANDLE;
    }

    process = (StProcess_InternalRef)StProcess_GetListHead();
    while (process) {
        if (process->gnt_node == process_node &&
            !StRefControlBlock_IsDying(&process->ref_control)) {
            if (process_out) *process_out = (StProcess_BorrowedRef)process;
            return STATUS_SUCCESS;
        }

        process = process->next;
    }

    return STATUS_ENTRY_NOT_FOUND;
}
