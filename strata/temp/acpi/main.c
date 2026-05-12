#include <stdint.h>

#include <uacpi/acpi.h>
#include <uacpi/event.h>
#include <uacpi/internal/tables.h>
#include <uacpi/platform/types.h>
#include <uacpi/sleep.h>
#include <uacpi/status.h>
#include <uacpi/tables.h>
#include <uacpi/types.h>
#include <uacpi/uacpi.h>

#include <strata/arch/interrupt.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/log.h>
#include <strata/macros.h>
#include <strata/panic.h>
#include <strata/status.h>
#include <strata/utf.h>

#include <ioapic.h>

#include "internal.h"

#define MODULE_NAME "acpi"

#define MAKE_UACPI_STATUS(uacpi_status)                                                            \
    ((uacpi_status)                                                                                \
         ? MAKE_STATUS(MAKE_BASE_STATUS(1, uacpi_status, STATUS_AREA_ACPI), STATUS_ATTR_NONE)      \
         : STATUS_SUCCESS)

uacpi_phys_addr g_rsdp_base;

static uacpi_iteration_decision iterate_ioapic_entry(
    uacpi_handle data __in, struct acpi_entry_hdr *entry __in
)
{
    StStatus *status_out = (StStatus *)data;
    StStatus status;
    struct acpi_madt_ioapic *ioapic;

    if (entry->type != ACPI_MADT_ENTRY_TYPE_IOAPIC) return UACPI_ITERATION_DECISION_CONTINUE;

    ioapic = (struct acpi_madt_ioapic *)entry;
    status = StIoapicP_Add(ioapic->id, ioapic->gsi_base, ioapic->address);
    if (!CHECK_SUCCESS(status)) {
        *status_out = status;
        return UACPI_ITERATION_DECISION_BREAK;
    }

    LOG_INFO(
        LM_CAT_ACPI,
        "added IOAPIC: ID=%d, GSI=%d, MMIO=0x%lx\n",
        ioapic->id,
        ioapic->gsi_base,
        (uintptr_t)ioapic->address
    );

    return UACPI_ITERATION_DECISION_CONTINUE;
}

struct iso_iter_data {
    StStatus status;
    uint32_t redirected_gsi_bitmap;
    uint16_t redirected_legacy_irq_bitmap;
};

enum {
    MADT_POLARITY_MASK = 0x3,
    MADT_POLARITY_ACTIVE_LOW = 0x3,
    MADT_TRIGGERING_MASK = 0xC,
    MADT_TRIGGERING_LEVEL = 0xC,
};

static uacpi_iteration_decision iterate_iso_entry(
    uacpi_handle data __in, struct acpi_entry_hdr *entry __in
)
{
    struct iso_iter_data *iter_data = (struct iso_iter_data *)data;
    StStatus status;
    struct acpi_madt_interrupt_source_override *iso;
    uint16_t flags = DLV_MODE_NORMAL | RFLAGS_INT_MASK;

    if (entry->type != ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE) {
        return UACPI_ITERATION_DECISION_CONTINUE;
    }

    iso = (struct acpi_madt_interrupt_source_override *)entry;

    if ((iso->flags & MADT_POLARITY_MASK) == MADT_POLARITY_ACTIVE_LOW) {
        flags |= RFLAGS_ACTIVE_LOW;
    }
    if ((iso->flags & MADT_TRIGGERING_MASK) == MADT_TRIGGERING_LEVEL) {
        flags |= RFLAGS_LEVEL_TRIGGERED;
    }

    status = StIoapicP_RouteGsiToVector(iso->gsi, 0x20 + iso->source, 0, flags);
    if (!CHECK_SUCCESS(status)) {
        iter_data->status = status;
        return UACPI_ITERATION_DECISION_BREAK;
    }

    iter_data->redirected_legacy_irq_bitmap |= (1 << iso->source);

    if (iso->gsi < 32) {
        iter_data->redirected_gsi_bitmap |= (1 << iso->gsi);
    }

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static StStatus init_uacpi(void)
{
    StStatus status;
    uacpi_status uacpi_status;
    struct uacpi_table table;
    struct iso_iter_data iso_iter_data = {
        0,
    };

    uacpi_status = uacpi_initialize(0);
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(LM_CAT_ACPI, "uacpi_initialize error: %s", uacpi_status_to_string(uacpi_status));
        return MAKE_UACPI_STATUS(uacpi_status);
    }

    /* query MADT */
    uacpi_status = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE, &table);
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(
            LM_CAT_UNCLASSIFIED,
            "could not find ACPI MADT: %s\n",
            uacpi_status_to_string(uacpi_status)
        );
        return MAKE_UACPI_STATUS(uacpi_status);
    }

    /* register I/O APIC */
    status = STATUS_SUCCESS;
    uacpi_status =
        uacpi_for_each_subtable(table.hdr, sizeof(struct acpi_madt), iterate_ioapic_entry, &status);
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(
            LM_CAT_UNCLASSIFIED,
            "an error occured while iterating I/O APIC entries in MADT: %s\n",
            uacpi_status_to_string(uacpi_status)
        );
        return MAKE_UACPI_STATUS(uacpi_status);
    }
    if (!CHECK_SUCCESS(status)) return status;

    /* override interrupt sources */
    uacpi_status = uacpi_for_each_subtable(
        table.hdr,
        sizeof(struct acpi_madt),
        iterate_iso_entry,
        &iso_iter_data
    );
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(
            LM_CAT_UNCLASSIFIED,
            "an error occured while iterating interrupt source override entries in MADT: %s\n",
            uacpi_status_to_string(uacpi_status)
        );
        return MAKE_UACPI_STATUS(uacpi_status);
    }
    if (!CHECK_SUCCESS(iso_iter_data.status)) return iso_iter_data.status;

    for (int i = 0; i < 16; i++) {
        if (iso_iter_data.redirected_legacy_irq_bitmap & (1 << i)) continue;
        if (iso_iter_data.redirected_gsi_bitmap & (1 << i)) continue;

        status = StIoapicP_RouteGsiToVector(i, 0x20 + i, 0, DLV_MODE_NORMAL | RFLAGS_INT_MASK);
        if (!CHECK_SUCCESS(status)) return status;
    }

    uacpi_status = uacpi_table_unref(&table);
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(
            LM_CAT_UNCLASSIFIED,
            "could not unref MADT: %s\n",
            uacpi_status_to_string(uacpi_status)
        );
        return MAKE_UACPI_STATUS(uacpi_status);
    }

    // TODO: loading and initialization of namespace should be late-executed
    uacpi_status = uacpi_namespace_load();
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(
            LM_CAT_ACPI,
            "uacpi_namespace_load error: %s",
            uacpi_status_to_string(uacpi_status)
        );
        return MAKE_UACPI_STATUS(uacpi_status);
    }

    uacpi_status = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(
            LM_CAT_ACPI,
            "uacpi_namespace_initialize error: %s",
            uacpi_status_to_string(uacpi_status)
        );
        return MAKE_UACPI_STATUS(uacpi_status);
    }

    uacpi_status = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(
            LM_CAT_ACPI,
            "uACPI GPE initialization error: %s",
            uacpi_status_to_string(uacpi_status)
        );
        return MAKE_UACPI_STATUS(uacpi_status);
    }

    return STATUS_SUCCESS;
}

static void notify_apic_mode(void)
{
    uacpi_status uacpi_status;
    uacpi_object *arg = uacpi_object_create_integer(1);
    uacpi_object_array args = {
        .count = 1,
        .objects = &arg,
    };

    uacpi_status = uacpi_execute(UACPI_NULL, "\\_PIC", &args);

    if (uacpi_status == UACPI_STATUS_OK) {
        LOG_INFO(LM_CAT_ACPI, "Successfully notified ACPI of APIC mode (\\_PIC(1))\n");
    } else if (uacpi_status == UACPI_STATUS_NOT_FOUND) {
        LOG_INFO(LM_CAT_ACPI, "\\_PIC method not found. Hardware is APIC-only.\n");
    } else {
        LOG_WARN(
            LM_CAT_ACPI,
            "Failed to evaluate \\_PIC: %s\n",
            uacpi_status_to_string(uacpi_status)
        );
    }

    uacpi_object_unref(arg);
}

struct table_iter_data {
    struct StGnt_Node *tables_node;
    StStatus status;
};

static uacpi_iteration_decision add_table_node(
    void *user __in, struct uacpi_installed_table *table __in, unsigned long idx __in
)
{
    StStatus status;
    struct table_iter_data *iter_data = (struct table_iter_data *)user;
    struct StGnt_Node *table_node;
    St_Utf32Char table_name[sizeof(table->hdr.signature) + 1] = {
        0,
    };

    status = StUtf_Utf8ToUtf32(
        (const St_Utf8Char *)table->hdr.signature,
        sizeof(table->hdr.signature),
        table_name,
        ARRAY_SIZE(table_name),
        NULL
    );
    if (!CHECK_SUCCESS(status)) {
        iter_data->status = status;
        return UACPI_ITERATION_DECISION_BREAK;
    }

    status = StGnt_AddNode(iter_data->tables_node, table_name, &table_node);
    if (!CHECK_SUCCESS(status)) {
        iter_data->status = status;
        return UACPI_ITERATION_DECISION_BREAK;
    }

    if (uacpi_signatures_match(table->hdr.signature, ACPI_MCFG_SIGNATURE)) {
        status = StAcpiTableMcfgIf_RegisterNode(table_node, table, idx);
        if (!CHECK_SUCCESS(status)) {
            iter_data->status = status;
            return UACPI_ITERATION_DECISION_BREAK;
        }
    }

    return UACPI_ITERATION_DECISION_CONTINUE;
}

static StStatus register_gnt_nodes(struct StGnt_Node *parent_node __in)
{
    StStatus status;
    uacpi_status uacpi_status;
    struct table_iter_data table_iter_data;
    struct StGnt_Node *gnt_acpi_root_dir, *gnt_tables_dir;

    status = StGnt_AddNode(parent_node, U"ACPI", &gnt_acpi_root_dir);
    if (!CHECK_SUCCESS(status)) return status;
    gnt_acpi_root_dir->type = GNT_NODETYPE_DIRECTORY;

    status = StGnt_AddNode(gnt_acpi_root_dir, U"Tables", &gnt_tables_dir);
    if (!CHECK_SUCCESS(status)) return status;
    gnt_tables_dir->type = GNT_NODETYPE_DIRECTORY;

    table_iter_data.tables_node = gnt_tables_dir;
    table_iter_data.status = STATUS_SUCCESS;
    uacpi_status = uacpi_for_each_table(0, add_table_node, &table_iter_data);
    if (uacpi_unlikely_error(uacpi_status)) {
        return MAKE_UACPI_STATUS(uacpi_status);
    }
    if (!CHECK_SUCCESS(table_iter_data.status)) {
        return table_iter_data.status;
    }

    return STATUS_SUCCESS;
}

static uacpi_interrupt_ret power_button_handler(uacpi_handle handle)
{
    uacpi_status uacpi_status;

    LOG_INFO(LM_CAT_ACPI, "Shutting down system...\n");

    // TODO: Notify kernel to power off

    // TODO: hand-off to the non-IRQ context
    uacpi_status = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(
            LM_CAT_ACPI,
            "Failed to prepare for S5: %s\n",
            uacpi_status_to_string(uacpi_status)
        );
        return UACPI_INTERRUPT_NOT_HANDLED;
    }

    StA_DisableInterrupt();

    uacpi_status = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
    LOG_ERROR(LM_CAT_ACPI, "System did not power off: %s\n", uacpi_status_to_string(uacpi_status));

    return UACPI_INTERRUPT_HANDLED;
}

StStatus acpi_module_main(uint64_t rsdp_base __in)
{
    StStatus status;
    struct StGnt_Node *gnt_system_hardware_node;
    struct StGnt_Node *gnt_system_firmware_node;

    status = StGnt_ResolvePath(NULL, U"/System/Hardware", &gnt_system_hardware_node);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to resolve path /System/Hardware");
    }

    status = StGnt_ResolvePath(NULL, U"/System/Firmware", &gnt_system_firmware_node);
    if (!CHECK_SUCCESS(status)) {
        St_Panic(status, "failed to resolve path /System/Firmware");
    }

    g_rsdp_base = (uacpi_phys_addr)rsdp_base;

    status = init_uacpi();
    if (!CHECK_SUCCESS(status)) return status;

    notify_apic_mode();

    status = register_gnt_nodes(gnt_system_firmware_node);
    if (!CHECK_SUCCESS(status)) return status;

    status = uacpi_install_fixed_event_handler(
        UACPI_FIXED_EVENT_POWER_BUTTON,
        power_button_handler,
        NULL
    );
    if (uacpi_unlikely_error(status)) {
        LOG_ERROR(
            LM_CAT_ACPI,
            "Failed to install power button handler: %s\n",
            uacpi_status_to_string(status)
        );
    }

    return STATUS_SUCCESS;
}
