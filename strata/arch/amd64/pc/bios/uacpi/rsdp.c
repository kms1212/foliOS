#include <uacpi/kernel_api.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <uacpi/acpi.h>

#include <strata/plat/bios.h>
#include <strata/plat/mm.h>

#include <strata/mm.h>
#include <strata/mm/types.h>
#include <strata/status.h>

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *rsdp_paddr_out)
{
    static int rsdp_paddr_found = 0;
    static uacpi_phys_addr rsdp_paddr;

    StStatus status;
    St_VirtPage cvmem_vpn;
    const void *bios_base;
    const struct StBiosP_Bda *bda;
    const struct StBiosP_ExtendedBda *ebda;
    size_t ebda_size;
    int rsdp_found = 0;
    const void *rsdp_addr;

    if (rsdp_paddr_found) {
        *rsdp_paddr_out = rsdp_paddr;
        return UACPI_STATUS_OK;
    }

    status = StMmP_MapConventionalMemory(&cvmem_vpn);
    if (!CHECK_SUCCESS(status)) return status;

    bda = PAGE_TO_VPTR(cvmem_vpn);
    if (!bda->ebda_segment) goto skip_ebda;

    ebda = (const void *)((uintptr_t)bda->ebda_segment << 4);
    ebda_size = (size_t)ebda->ebda_size_kb * 1024;
    if (!ebda_size) goto skip_ebda;

    for (size_t i = 0; i < ebda_size - sizeof(ACPI_RSDP_SIGNATURE) + 1; i++) {
        if (memcmp(
                &((const char *)ebda)[i],
                ACPI_RSDP_SIGNATURE,
                sizeof(ACPI_RSDP_SIGNATURE) - 1
            ) == 0) {
            rsdp_found = 1;
            rsdp_addr = &((const char *)ebda)[i];
            break;
        }
    }

skip_ebda:
    bios_base = (const void *)(PAGE_TO_ADDR(cvmem_vpn) + 0xF0000);

    for (size_t i = 0; i < 0x10000 - sizeof(ACPI_RSDP_SIGNATURE) + 1; i++) {
        if (memcmp(
                &((const char *)bios_base)[i],
                ACPI_RSDP_SIGNATURE,
                sizeof(ACPI_RSDP_SIGNATURE) - 1
            ) == 0) {
            rsdp_found = 1;
            rsdp_addr = &((const char *)bios_base)[i];
            break;
        }
    }

    if (!rsdp_found) return UACPI_STATUS_NOT_FOUND;

    status = StMm_GlobalVirtAddrToPhysAddr((uintptr_t)rsdp_addr, &rsdp_paddr);
    if (!CHECK_SUCCESS(status)) return status;

    rsdp_paddr_found = 1;

    *rsdp_paddr_out = rsdp_paddr;

    return UACPI_STATUS_OK;
}
