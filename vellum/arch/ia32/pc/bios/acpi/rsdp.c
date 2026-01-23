#include <uacpi/kernel_api.h>
#include <uacpi/acpi.h>

#include <stddef.h>
#include <stdint.h>

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
    static const void *rsdp_addr = NULL;
    uint16_t rsdp_base_seg;
    const void *ebda_ptr = NULL;
    size_t ebda_size;
    int match;

    if (rsdp_addr) {
        *out_rsdp_address = (uacpi_phys_addr)rsdp_addr;

        return UACPI_STATUS_OK;
    }

    rsdp_base_seg = *(uint16_t *)0x40E;
    if (!rsdp_base_seg) goto skip_ebda;
    ebda_ptr = (const void *)(rsdp_base_seg << 4);
    ebda_size = (size_t)*(const uint16_t *)ebda_ptr * 1024;
    if (!ebda_size) goto skip_ebda;

    for (const char *ptr = ebda_ptr; (uintptr_t)ptr < (uintptr_t)ebda_ptr + ebda_size; ptr++) {
        match = 1;

        for (int i = 0; i < 8; i++) {
            if (ptr[i] != ACPI_RSDP_SIGNATURE[i]) {
                match = 0;
                break;
            }
        }

        if (match) {
            rsdp_addr = (const void *)ptr;
            break;
        }
    }

    if (rsdp_addr) {
        *out_rsdp_address = (uacpi_phys_addr)rsdp_addr;

        return UACPI_STATUS_OK;
    }

skip_ebda:
    for (const char *ptr = (const char *)0x000E0000; (uintptr_t)ptr < 0x00100000; ptr++) {
        match = 1;

        for (int i = 0; i < 8; i++) {
            if (ptr[i] != ACPI_RSDP_SIGNATURE[i]) {
                match = 0;
                break;
            }
        }

        if (match) {
            rsdp_addr = (const void *)ptr;
            break;
        }
    }

    if (rsdp_addr) {
        *out_rsdp_address = (uacpi_phys_addr)rsdp_addr;

        return UACPI_STATUS_OK;
    }


    return UACPI_STATUS_NOT_FOUND;
}
