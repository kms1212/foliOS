#include "internal.h"

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/handle.h>
#include <strata/status.h>

StStatus StProcessGnt_DispatchCallArgs(
    struct StGnt_Node *node __in, StHandle_Id handle __in, uint32_t funcid __in, const long args[4]
)
{
    StStatus status;

    if (!node || !args) return STATUS_INVALID_VALUE;

    status = StProcessIf_DispatchCallArgs(node, handle, funcid, args);
    if (status != STATUS_NOT_SUPPORTED) return status;

    status = StThreadIf_DispatchCallArgs(node, handle, funcid, args);
    if (status != STATUS_NOT_SUPPORTED) return status;

    return STATUS_NOT_SUPPORTED;
}
