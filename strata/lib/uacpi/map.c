#include <uacpi/kernel_api.h>

#include <strata/mm.h>

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len)
{
    St_VirtPage vpn;

    StMm_MapGlobal(
        VMM_DOMAIN_IO,
        &vpn,
        ADDR_TO_FRAME(addr),
        (St_PageCount)ALIGN_DIV((addr % PAGE_SIZE) + len, PAGE_SIZE),
        NULL,
        (struct StMm_CompoundFlags){AF_DEFAULT, MF_KERNEL_DEFAULT}
    );

    return (void *)(PAGE_TO_ADDR(vpn) + (addr % PAGE_SIZE));
}

void uacpi_kernel_unmap(void *addr, uacpi_size len)
{
    StMm_UnmapGlobal(VMM_DOMAIN_IO, VPTR_TO_PAGE(addr), (St_PageCount)ALIGN_DIV(len, PAGE_SIZE));
}

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle)
{
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {}
