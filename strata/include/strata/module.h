#ifndef __STRATA_MODULE_H__
#define __STRATA_MODULE_H__

#include <strata/process.h>
#include <strata/status.h>

typedef int StModule_Id __nocast;

struct StModule {
    struct StModule *next;

    StModule_Id id;

    struct StProcess *process;
};

#endif  // __STRATA_MODULE_H__
