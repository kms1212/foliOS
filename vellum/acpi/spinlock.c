#include <uacpi/kernel_api.h>

#include <stdlib.h>

uacpi_handle uacpi_kernel_create_spinlock(void)
{
    return malloc(1);
}
void uacpi_kernel_free_spinlock(uacpi_handle spinlock)
{
    free(spinlock);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle spinlock)
{
    return UACPI_STATUS_OK;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle spinlock, uacpi_cpu_flags cpuflags)
{

}
