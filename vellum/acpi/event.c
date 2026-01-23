#include <uacpi/kernel_api.h>

#include <stdlib.h>

uacpi_handle uacpi_kernel_create_event(void)
{
    return malloc(1);
}

void uacpi_kernel_free_event(uacpi_handle event)
{
    free(event);
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle event, uacpi_u16 timeout)
{
    return UACPI_TRUE;
}

void uacpi_kernel_signal_event(uacpi_handle event)
{
    
}

void uacpi_kernel_reset_event(uacpi_handle event)
{
    
}
