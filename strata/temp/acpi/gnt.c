#include "internal.h"

#include <strata/compiler.h>
#include <strata/module.h>
#include <strata/panic.h>
#include <strata/status.h>

struct StModule *StAcpi_Module;

__constructor static void init_acpi_module(void)
{
    StStatus status;

    status = StModule_Create(&StAcpi_Module);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "Failed to create ACPI module");
    }

    StAcpi_Module->dispatch_args = StAcpiGnt_DispatchCallArgs;
}
