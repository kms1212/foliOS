#ifndef __STRATA_PROCESS_INTERNAL_H__
#define __STRATA_PROCESS_INTERNAL_H__

#include <strata/gnt.h>
#include <strata/handle.h>
#include <strata/process.h>
#include <strata/thread.h>

StStatus StProcessGnt_Resolve(
    struct StGnt_Node *base_node __in,
    const St_Utf32Char *path __in,
    struct StGnt_Node **next_node __out,
    const St_Utf32Char **remaining_path __out
);

StStatus StProcessGnt_Iterate(
    struct StGnt_Node *parent __in,
    uint64_t cookie __in,
    void *buffer __in,
    size_t buffer_size __in,
    size_t *entry_count __out,
    uint64_t *next_cookie __out
);

StStatus StProcessGnt_DispatchCallArgs(
    struct StGnt_Node *node __in,
    StHandle_Id handle __in,
    uint32_t funcid __in,
    const long args[4]
);

StStatus StProcessIf_DispatchCallArgs(
    struct StGnt_Node *node __in,
    StHandle_Id handle __in,
    uint32_t funcid __in,
    const long args[4]
);

StStatus StThreadIf_DispatchCallArgs(
    struct StGnt_Node *node __in,
    StHandle_Id handle __in,
    uint32_t funcid __in,
    const long args[4]
);

#endif  // __STRATA_PROCESS_INTERNAL_H__
