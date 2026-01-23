#include <uacpi/kernel_api.h>

#include <stdlib.h>

void *uacpi_kernel_alloc(uacpi_size size)
{
    return malloc(size);
}

void uacpi_kernel_free(void *mem)
{
    free(mem);
}
