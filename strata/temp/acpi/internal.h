#ifndef __STRATA_TEMP_ACPI_INTERNAL_H__
#define __STRATA_TEMP_ACPI_INTERNAL_H__

#include <stdint.h>

#include <uacpi/internal/tables.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/handle.h>
#include <strata/module.h>
#include <strata/status.h>

extern struct StModule *StAcpi_Module;

StStatus StAcpiGnt_DispatchCallArgs(
    StGnt_Node_StrongRef node __in,
    StHandle_Id handle __in,
    uint32_t funcid __in,
    const long args[4]
);

StStatus StAcpiTableMcfgIf_DispatchCallArgs(
    StGnt_Node_StrongRef node __in,
    StHandle_Id handle __in,
    uint32_t funcid __in,
    const long args[4]
);

StStatus StAcpiTableMcfgIf_RegisterNode(
    StGnt_Node_StrongRef table_node __inout,
    struct uacpi_installed_table *table __in,
    unsigned long table_index __in
);

#endif  // __STRATA_TEMP_ACPI_INTERNAL_H__
