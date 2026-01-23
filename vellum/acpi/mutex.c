#include <uacpi/kernel_api.h>

#include <stdlib.h>

uacpi_handle uacpi_kernel_create_mutex(void)
{
    return malloc(1);
}
void uacpi_kernel_free_mutex(uacpi_handle mutex)
{
    return free(mutex);
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle mutex, uacpi_u16 timeout)
{
    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle mutex)
{

}
