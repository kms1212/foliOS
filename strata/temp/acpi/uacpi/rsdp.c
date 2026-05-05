#include <uacpi/kernel_api.h>

#include <uacpi/status.h>

#define MODULE_NAME "acpi"

extern uacpi_phys_addr g_rsdp_base;

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *rsdp_paddr_out)
{
    if (!g_rsdp_base) return UACPI_STATUS_NOT_FOUND;

    *rsdp_paddr_out = g_rsdp_base;

    return UACPI_STATUS_OK;
}
