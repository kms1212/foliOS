#ifndef __STRATA_TEMP_ACPI_TABLE_MCFG_IF_H__
#define __STRATA_TEMP_ACPI_TABLE_MCFG_IF_H__

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/status.h>

StStatus StAcpi_TableMcfgIf_RegisterNode(struct StGnt_Node *table_node __inout);

#endif  // __STRATA_TEMP_ACPI_TABLE_MCFG_IF_H__
