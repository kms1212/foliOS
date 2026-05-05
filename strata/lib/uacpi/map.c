#include <uacpi/kernel_api.h>

#include <strata/log.h>
#include <strata/mm.h>

#define MODULE_NAME "acpi"

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len)
{
    StStatus status;
    St_VirtPage vpn;

    status = StMm_MapGlobal(
        VMM_DOMAIN_IO,
        &vpn,
        ADDR_TO_FRAME(addr),
        (St_PageCount)ALIGN_DIV((addr % PAGE_SIZE) + len, PAGE_SIZE),
        NULL,
        (struct StMm_CompoundFlags){AF_DEFAULT, MF_KERNEL_DEFAULT}
    );
    if (!CHECK_SUCCESS(status)) {
        LOG_ERROR(LM_CAT_UNCLASSIFIED, "StMm_MapGlobal() failed");
        return NULL;
    }

    return (void *)(PAGE_TO_ADDR(vpn) + (addr % PAGE_SIZE));
}

void uacpi_kernel_unmap(void *addr, uacpi_size len)
{
    StMm_UnmapGlobal(
        VMM_DOMAIN_IO,
        VPTR_TO_PAGE(addr),
        (St_PageCount)ALIGN_DIV(((uintptr_t)addr % PAGE_SIZE) + len, PAGE_SIZE)
    );
}
