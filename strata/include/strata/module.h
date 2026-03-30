#ifndef __STRATA_MODULE_H__
#define __STRATA_MODULE_H__

#include <strata/gnt.h>
#include <strata/process.h>
#include <strata/status.h>

typedef int StModule_Id __nocast;

struct StModule {
    struct StModule *next;

    StModule_Id id;

    struct StProcess *process;

    StGnt_ResolveFunc resolve;
};

StStatus StModule_Create(struct StModule **module __out);
void StModule_Remove(struct StModule *module __in);

#endif  // __STRATA_MODULE_H__
