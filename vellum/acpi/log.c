#include <uacpi/kernel_api.h>

#include <stdio.h>

#include <vellum/log.h>

#define MODULE_NAME "acpi"

void uacpi_kernel_log(uacpi_log_level ll, const uacpi_char *fmt, ...)
{
    va_list args;
    int internal_ll = ll;

    va_start(args, fmt);
    VlLog_PrintValist(internal_ll, MODULE_NAME, fmt, args);
    va_end(args);
}

void uacpi_kernel_vlog(uacpi_log_level ll, const uacpi_char *fmt, uacpi_va_list args)
{
    int internal_ll = ll;

    VlLog_PrintValist(internal_ll, MODULE_NAME, fmt, (va_list)args);
}
