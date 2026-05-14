#include "acpi_table_mcfg_if.h"

#include <assert.h>
#include <stdint.h>

#include <uacpi/acpi.h>
#include <uacpi/internal/tables.h>
#include <uacpi/status.h>
#include <uacpi/tables.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/gnt/interface.h>
#include <strata/gnt_refs.h>
#include <strata/handle.h>
#include <strata/mm/pool.h>
#include <strata/status.h>
#include <strata/uuid.h>

#include "sidl/mcfg.server.h"
#include "sidl/mcfg.types.h"

#include "internal.h"

#define MAKE_UACPI_STATUS(uacpi_status)                                                            \
    ((uacpi_status)                                                                                \
         ? MAKE_STATUS(MAKE_BASE_STATUS(1, uacpi_status, STATUS_AREA_ACPI), STATUS_ATTR_NONE)      \
         : STATUS_SUCCESS)

struct acpi_table_mcfg_node_context {
    struct uacpi_installed_table *table;
    unsigned long table_index;
};

struct acpi_table_mcfg_dispatch_context {
    struct StGnt_Node *node;
    struct uacpi_installed_table *table;
    unsigned long table_index;
};

static const struct StUuid g_mcfg_interface_uuid = UUID_ACPITABLEMCFG_INTERFACE_INIT;

static int is_mcfg_context(const struct acpi_table_mcfg_node_context *ctx)
{
    return ctx && ctx->table &&
        uacpi_signatures_match(ctx->table->hdr.signature, ACPI_MCFG_SIGNATURE);
}

static StStatus get_mcfg_context_from_node(
    struct StGnt_Node *node, struct acpi_table_mcfg_dispatch_context *ctx_out
)
{
    struct acpi_table_mcfg_node_context *node_ctx;

    if (!node || !ctx_out) return STATUS_INVALID_VALUE;

    node_ctx = (struct acpi_table_mcfg_node_context *)node->private_data;
    if (!is_mcfg_context(node_ctx)) return STATUS_NOT_SUPPORTED;

    ctx_out->node = node;
    ctx_out->table = node_ctx->table;
    ctx_out->table_index = node_ctx->table_index;

    return STATUS_SUCCESS;
}

static StStatus acquire_mcfg_table(
    struct acpi_table_mcfg_dispatch_context *ctx,
    uacpi_table *table_ref_out,
    const struct acpi_mcfg **mcfg_out
)
{
    uacpi_status uacpi_status;

    if (!ctx || !ctx->table || !table_ref_out || !mcfg_out) return STATUS_INVALID_VALUE;

    table_ref_out->index = ctx->table_index;
    table_ref_out->ptr = ctx->table->ptr;

    uacpi_status = uacpi_table_ref(table_ref_out);
    if (uacpi_unlikely_error(uacpi_status)) return MAKE_UACPI_STATUS(uacpi_status);

    if (!ctx->table->ptr) {
        uacpi_table_unref(table_ref_out);
        return STATUS_CONFLICTING_STATE;
    }

    *mcfg_out = (const struct acpi_mcfg *)ctx->table->ptr;

    return STATUS_SUCCESS;
}

static StStatus release_mcfg_table(StStatus status, uacpi_table *table_ref)
{
    uacpi_status uacpi_status;

    if (!table_ref) return status;

    uacpi_status = uacpi_table_unref(table_ref);
    if (CHECK_SUCCESS(status) && uacpi_unlikely_error(uacpi_status)) {
        status = MAKE_UACPI_STATUS(uacpi_status);
    }

    return status;
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

static StStatus mcfg_get_entry_count(
    void *context __inout, StHandle handle __in, uint32_t *count __out
)
{
    assert(context);
    assert(count);

    StStatus status;
    uacpi_table table_ref;
    const struct acpi_mcfg *mcfg = NULL;
    struct acpi_table_mcfg_dispatch_context *ctx =
        (struct acpi_table_mcfg_dispatch_context *)context;

    (void)handle;

    status = acquire_mcfg_table(ctx, &table_ref, &mcfg);
    if (!CHECK_SUCCESS(status)) return status;

    status = get_mcfg_entry_count_from_table(mcfg, count);

    return release_mcfg_table(status, &table_ref);
}

static StStatus mcfg_get_entry(
    void *context __inout,
    StHandle handle __in,
    uint32_t index __in,
    StIfAcpiTblMcfg_Entry *entry __out
)
{
    assert(context);
    assert(entry);

    StStatus status;
    uint32_t entry_count;
    uacpi_table table_ref;
    const struct acpi_mcfg *mcfg = NULL;
    const struct acpi_mcfg_allocation *allocation;
    struct acpi_table_mcfg_dispatch_context *ctx =
        (struct acpi_table_mcfg_dispatch_context *)context;

    (void)handle;

    status = acquire_mcfg_table(ctx, &table_ref, &mcfg);
    if (!CHECK_SUCCESS(status)) return status;

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
    return release_mcfg_table(status, &table_ref);
}

static const StIfAcpiTblMcfg_ServerVTable g_mcfg_vtable = {
    .GetEntryCount = mcfg_get_entry_count,
    .GetEntry = mcfg_get_entry,
};

StStatus StAcpiTableMcfgIf_DispatchCallArgs(
    StGnt_Node_StrongRef node __in,
    StHandle_Id handle __in,
    uint32_t funcid __in,
    const long args[4]
)
{
    StStatus status;
    struct acpi_table_mcfg_dispatch_context ctx;

    if (!node || !args) return STATUS_INVALID_VALUE;

    status = get_mcfg_context_from_node(node, &ctx);
    if (!CHECK_SUCCESS(status)) return status;

    return StIfAcpiTblMcfg_ServerDispatchArgs(&g_mcfg_vtable, &ctx, (StHandle)handle, funcid, args);
}

StStatus StAcpiTableMcfgIf_RegisterNode(
    StGnt_Node_StrongRef table_node, struct uacpi_installed_table *table, unsigned long table_index
)
{
    assert(table_node);

    StStatus status;
    struct acpi_table_mcfg_node_context *node_ctx;

    if (!table) return STATUS_INVALID_VALUE;
    if (!StAcpi_Module) return STATUS_CONFLICTING_STATE;
    if (!uacpi_signatures_match(table->hdr.signature, ACPI_MCFG_SIGNATURE)) {
        return STATUS_INVALID_VALUE;
    }
    if (table_node->private_data) return STATUS_CONFLICTING_STATE;

    status = StPool_AllocateClear(sizeof(*node_ctx), (void **)&node_ctx);
    if (!CHECK_SUCCESS(status)) return status;

    node_ctx->table = table;
    node_ctx->table_index = table_index;

    status =
        StGnt_RegisterInterface(table_node, &g_mcfg_interface_uuid, 0, STIFACPITBLMCFG_FUNCID_SPAN);
    if (!CHECK_SUCCESS(status)) {
        StPool_Free(node_ctx);
        return status;
    }

    table_node->type = GNT_NODETYPE_LEAF;
    table_node->private_data = node_ctx;
    table_node->handler_module = StAcpi_Module;

    return STATUS_SUCCESS;
}
