#include <stdio.h>

#include <uacpi/event.h>
#include <uacpi/sleep.h>
#include <uacpi/uacpi.h>

#include <strata/arch/interrupt.h>

#include <strata/log.h>

#define MODULE_NAME "acpi"

uacpi_phys_addr g_rsdp_base;

int acpi_module_main(uint64_t rsdp_base)
{
    uacpi_status uacpi_status;

    g_rsdp_base = (uacpi_phys_addr)rsdp_base;

    uacpi_status = uacpi_initialize(0);
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(LM_CAT_ACPI, "uacpi_initialize error: %s", uacpi_status_to_string(uacpi_status));
        return 1;
    }

    uacpi_status = uacpi_namespace_load();
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(
            LM_CAT_ACPI,
            "uacpi_namespace_load error: %s",
            uacpi_status_to_string(uacpi_status)
        );
        return 1;
    }

    uacpi_status = uacpi_namespace_initialize();
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(
            LM_CAT_ACPI,
            "uacpi_namespace_initialize error: %s",
            uacpi_status_to_string(uacpi_status)
        );
        return 1;
    }

    uacpi_status = uacpi_finalize_gpe_initialization();
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(
            LM_CAT_ACPI,
            "uACPI GPE initialization error: %s",
            uacpi_status_to_string(uacpi_status)
        );
        return 1;
    }

    return 0;
}

void acpi_module_shutdown(void)
{
    uacpi_status uacpi_status;

    uacpi_status = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    if (uacpi_unlikely_error(uacpi_status)) {
        LOG_ERROR(
            LM_CAT_UNCLASSIFIED,
            "Failed to prepare for S5: %s",
            uacpi_status_to_string(uacpi_status)
        );
        return;
    }

    StA_DisableInterrupt();

    uacpi_status = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
    LOG_ERROR(
        LM_CAT_UNCLASSIFIED,
        "System did not power off: %s",
        uacpi_status_to_string(uacpi_status)
    );
    for (;;) {
        StA_Hlt();
    }
}
