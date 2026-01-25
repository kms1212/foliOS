#include <uacpi/kernel_api.h>

#include <vellum/macros.h>
#include <vellum/mm.h>

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len)
{
    mm_map(addr / PAGE_SIZE, addr / PAGE_SIZE, ALIGN_DIV(addr % PAGE_SIZE + len, PAGE_SIZE), 0);

    return (void *)addr;
}

void uacpi_kernel_unmap(void *addr, uacpi_size len)
{
    // mm_unmap((uintptr_t)addr / PAGE_SIZE, ALIGN_DIV((uintptr_t)addr % PAGE_SIZE + len,
    // PAGE_SIZE));
}

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle)
{
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {}
