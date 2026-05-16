#ifndef __STRATA_PROCESS_INTERNAL_H__
#define __STRATA_PROCESS_INTERNAL_H__

#include <strata/gnt.h>
#include <strata/handle.h>
#include <strata/process.h>
#include <strata/thread.h>

StStatus StProcessGnt_Resolve(
    StGnt_Node_StrongRef base_node __in,
    const St_Utf32Char *path __in,
    StGnt_Node_StrongRef *next_node __out,
    const St_Utf32Char **remaining_path __out
);

StStatus StProcessGnt_Iterate(
    StGnt_Node_StrongRef parent __in,
    uint64_t cookie __in,
    void *buffer __in,
    size_t buffer_size __in,
    size_t *entry_count __out,
    uint64_t *next_cookie __out
);

StStatus StProcessGnt_DispatchCallArgs(
    StGnt_Node_StrongRef node __in,
    StHandle_Id handle __in,
    uint32_t funcid __in,
    const long args[4]
);

StStatus StProcessGnt_ParseProcessId(
    const St_Utf32Char *name __in, size_t name_len __in, StProcess_Id *process_id __out
);

enum StProcessGnt_NodeKind {
    PROCESS_GNT_NODE_STDIO_STDIN,
    PROCESS_GNT_NODE_STDIO_STDOUT,
    PROCESS_GNT_NODE_STDIO_STDERR,
};

struct StProcessGnt_NodeData {
    enum StProcessGnt_NodeKind kind;
};

extern struct StProcessGnt_NodeData StProcessGnt_StdinNodeData;
extern struct StProcessGnt_NodeData StProcessGnt_StdoutNodeData;
extern struct StProcessGnt_NodeData StProcessGnt_StderrNodeData;

int StProcessGnt_IsProcessRootNode(StGnt_Node_InternalRef node __in);

StStatus StProcessGnt_GetProcessFromNode(
    StGnt_Node_StrongRef process_node __in, StProcess_BorrowedRef *process_out __out_optional
);

StStatus StProcessIf_DispatchCallArgs(
    StGnt_Node_StrongRef node __in,
    StHandle_Id handle __in,
    uint32_t funcid __in,
    const long args[4]
);

StStatus StThreadIf_DispatchCallArgs(
    StGnt_Node_StrongRef node __in,
    StHandle_Id handle __in,
    uint32_t funcid __in,
    const long args[4]
);

StStatus StStdioIf_DispatchCallArgs(
    StGnt_Node_StrongRef node __in,
    StHandle_Id handle __in,
    uint32_t funcid __in,
    const long args[4]
);

#endif  // __STRATA_PROCESS_INTERNAL_H__
