#include "internal.h"

#include <strata/compiler.h>
#include <strata/module.h>
#include <strata/panic.h>
#include <strata/status.h>

struct StModule *StProcess_Module;

__constructor static void init_process_module(void)
{
    StStatus status;

    status = StModule_Create(&StProcess_Module);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "Failed to create process module");
    }

    StProcess_Module->resolve = StProcessGnt_Resolve;
    StProcess_Module->list = StProcessGnt_Iterate;
    StProcess_Module->dispatch_args = StProcessGnt_DispatchCallArgs;
}
