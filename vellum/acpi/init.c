#include <uacpi/kernel_api.h>

#include <vellum/log.h>

#define MODULE_NAME "acpi"

uacpi_status uacpi_kernel_initialize(uacpi_init_level current_init_lvl)
{
    switch (current_init_lvl) {
        case UACPI_INIT_LEVEL_EARLY:
            LOG_INFO("uACPI early initialized");
            break;
        case UACPI_INIT_LEVEL_SUBSYSTEM_INITIALIZED:
            LOG_INFO("uACPI subsystem initialized");
            break;
        case UACPI_INIT_LEVEL_NAMESPACE_LOADED:
            LOG_INFO("uACPI namespace loaded");
            break;
        case UACPI_INIT_LEVEL_NAMESPACE_INITIALIZED:
            LOG_INFO("uACPI namespace initialized");
            break;
    }

    return UACPI_STATUS_OK;
}

void uacpi_kernel_deinitialize(void) {}
