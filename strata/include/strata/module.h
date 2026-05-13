#ifndef __STRATA_MODULE_H__
#define __STRATA_MODULE_H__

#include <stdint.h>

#include <strata/gnt.h>
#include <strata/handle.h>
#include <strata/process.h>
#include <strata/status.h>

typedef int StModule_Id __nocast;

typedef StStatus (*StModule_DispatchArgsFunc)(
    StGnt_Node_StrongRef node __in,
    StHandle_Id handle __in,
    uint32_t funcid __in,
    const long args[4]
);

struct StModule {
    struct StModule *next;

    StModule_Id id;

    StProcess_StrongRef process;

    StGnt_ResolveFunc resolve;
    StGnt_IterateFunc list;
    StModule_DispatchArgsFunc dispatch_args;
};

StStatus StModule_Create(struct StModule **module __out);
void StModule_Remove(struct StModule *module __in);

#endif  // __STRATA_MODULE_H__
