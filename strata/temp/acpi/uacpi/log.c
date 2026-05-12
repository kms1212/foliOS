#include <uacpi/kernel_api.h>

#include <stdarg.h>

#include <uacpi/log.h>
#include <uacpi/platform/types.h>

#include <strata/log.h>

#define MODULE_NAME "acpi"

void uacpi_kernel_log(uacpi_log_level ll, const uacpi_char *fmt, ...)
{
    va_list args;
    int internal_ll = ll;

    va_start(args, fmt);
    VLOG(internal_ll, LM_CAT_ACPI, fmt, args);
    va_end(args);
}

void uacpi_kernel_vlog(uacpi_log_level ll, const uacpi_char *fmt, uacpi_va_list args)
{
    int internal_ll = ll;

    VLOG(internal_ll, LM_CAT_ACPI, fmt, args);
}
