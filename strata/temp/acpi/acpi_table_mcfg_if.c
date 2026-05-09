#include "acpi_table_mcfg_if.h"

#include <stdint.h>
#include <string.h>

#include <uacpi/acpi.h>
#include <uacpi/internal/tables.h>
#include <uacpi/status.h>
#include <uacpi/tables.h>
#include <uacpi/types.h>

#include <strata/gnt/interface.h>
#include <strata/module.h>
#include <strata/panic.h>
#include <strata/utf.h>

#include "mcfg.server.h"

#define MAKE_UACPI_STATUS(uacpi_status)                                                            \
    ((uacpi_status)                                                                                \
         ? MAKE_STATUS(MAKE_BASE_STATUS(1, uacpi_status, STATUS_AREA_ACPI), STATUS_ATTR_NONE)      \
         : STATUS_SUCCESS)

static struct StModule *g_acpi_table_mcfg_module;
static const struct StUuid g_mcfg_interface_uuid = UUID_ACPITABLEMCFG_INTERFACE_INIT;

static int is_mcfg_node(const struct StGnt_Node *node)
{
    return node && node->name_len == 4 &&
        memcmp(node->name, U"MCFG", 4 * sizeof(St_Utf32Char)) == 0;
}

static StStatus get_mcfg_entry_count_from_table(const struct acpi_mcfg *mcfg, uint32_t *count_out)
{
    uint32_t payload_size;

    if (!mcfg || !count_out) return STATUS_INVALID_VALUE;
    if (mcfg->hdr.length < sizeof(*mcfg)) return STATUS_INVALID_FORMAT;

    payload_size = mcfg->hdr.length - sizeof(*mcfg);
    if (payload_size % sizeof(struct acpi_mcfg_allocation) != 0) {
        return STATUS_INVALID_FORMAT;
    }

    *count_out = payload_size / sizeof(struct acpi_mcfg_allocation);

    return STATUS_SUCCESS;
}

static StStatus find_mcfg_table(uacpi_table *table_out)
{
    uacpi_status uacpi_status;

    if (!table_out) return STATUS_INVALID_VALUE;

    uacpi_status = uacpi_table_find_by_signature(ACPI_MCFG_SIGNATURE, table_out);
    return MAKE_UACPI_STATUS(uacpi_status);
}

static StStatus mcfg_get_entry_count(
    void *context __inout, StHandle handle __in, uint32_t *count __out
)
{
    StStatus status;
    uacpi_status uacpi_status;
    uacpi_table table;

    (void)context;
    (void)handle;

    if (!count) return STATUS_INVALID_VALUE;

    status = find_mcfg_table(&table);
    if (!CHECK_SUCCESS(status)) return status;

    status = get_mcfg_entry_count_from_table((const struct acpi_mcfg *)table.ptr, count);

    uacpi_status = uacpi_table_unref(&table);
    if (CHECK_SUCCESS(status) && uacpi_unlikely_error(uacpi_status)) {
        status = MAKE_UACPI_STATUS(uacpi_status);
    }

    return status;
}

static StStatus mcfg_get_entry(
    void *context __inout,
    StHandle handle __in,
    uint32_t index __in,
    StIfAcpiTblMcfg_Entry *entry __out
)
{
    StStatus status;
    uint32_t entry_count;
    const struct acpi_mcfg *mcfg;
    const struct acpi_mcfg_allocation *allocation;
    uacpi_status uacpi_status;
    uacpi_table table;

    (void)context;
    (void)handle;

    if (!entry) return STATUS_INVALID_VALUE;

    status = find_mcfg_table(&table);
    if (!CHECK_SUCCESS(status)) return status;

    mcfg = (const struct acpi_mcfg *)table.ptr;
    status = get_mcfg_entry_count_from_table(mcfg, &entry_count);
    if (!CHECK_SUCCESS(status)) goto cleanup;
    if (index >= entry_count) {
        status = STATUS_ENTRY_NOT_FOUND;
        goto cleanup;
    }

    allocation = &mcfg->entries[index];
    entry->base_address = allocation->address;
    entry->pci_segment_group = allocation->segment;
    entry->start_bus = allocation->start_bus;
    entry->end_bus = allocation->end_bus;

    status = STATUS_SUCCESS;

cleanup:
    uacpi_status = uacpi_table_unref(&table);
    if (CHECK_SUCCESS(status) && uacpi_unlikely_error(uacpi_status)) {
        status = MAKE_UACPI_STATUS(uacpi_status);
    }

    return status;
}

static const StIfAcpiTblMcfg_ServerVTable g_mcfg_vtable = {
    .GetEntryCount = mcfg_get_entry_count,
    .GetEntry = mcfg_get_entry,
};

static StStatus acpi_table_mcfg_dispatch_call_args(
    struct StGnt_Node *node __in, StHandle_Id handle __in, uint32_t funcid __in, const long args[4]
)
{
    if (!node || !args) return STATUS_INVALID_VALUE;
    if (!is_mcfg_node(node)) return STATUS_NOT_SUPPORTED;

    return StIfAcpiTblMcfg_ServerDispatchArgs(&g_mcfg_vtable, NULL, handle, funcid, args);
}

__constructor static void init_acpi_table_mcfg_if(void)
{
    StStatus status;

    status = StModule_Create(&g_acpi_table_mcfg_module);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "Failed to create ACPI MCFG interface module");
    }

    g_acpi_table_mcfg_module->dispatch_args = acpi_table_mcfg_dispatch_call_args;
}

StStatus StAcpi_TableMcfgIf_RegisterNode(struct StGnt_Node *table_node)
{
    StStatus status;

    if (!table_node) return STATUS_INVALID_VALUE;
    if (!g_acpi_table_mcfg_module) return STATUS_CONFLICTING_STATE;

    status = StGnt_RegisterInterface(
        table_node,
        &g_mcfg_interface_uuid,
        0,
        STIFACPITBLMCFG_FUNCID_SPAN
    );
    if (!CHECK_SUCCESS(status)) return status;

    table_node->handler_module = g_acpi_table_mcfg_module;

    return STATUS_SUCCESS;
}
