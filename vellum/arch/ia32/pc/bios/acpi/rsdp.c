#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <vellum/acpi.h>

static int has_valid_checksum(const void *ptr, size_t size)
{
    const uint8_t *bytes = ptr;
    uint8_t sum = 0;

    for (size_t i = 0; i < size; i++) {
        sum += bytes[i];
    }

    return sum == 0;
}

static int is_valid_rsdp(const struct acpi_rsdp *rsdp)
{
    if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0) return 0;
    if (!has_valid_checksum(rsdp, 20)) return 0;
    if (rsdp->revision >= 2 && !has_valid_checksum(rsdp, rsdp->length)) return 0;

    return 1;
}

status_t VlAcpi_FindRsdp(const struct acpi_rsdp **out_rsdp)
{
    static const struct acpi_rsdp *rsdp_addr = NULL;
    uint16_t rsdp_base_seg;
    const void *ebda_ptr = NULL;
    size_t ebda_size;

    if (rsdp_addr) {
        if (out_rsdp) *out_rsdp = rsdp_addr;

        return STATUS_SUCCESS;
    }

    uint16_t *rsdp_base_seg_ptr = (uint16_t *)0x40E;

    // a workaround to make the compiler shut up in release build
    __asm__ volatile("" : "+g"(rsdp_base_seg_ptr));

    rsdp_base_seg = *rsdp_base_seg_ptr;

    if (!rsdp_base_seg) goto skip_ebda;
    ebda_ptr = (const void *)(rsdp_base_seg << 4);
    ebda_size = (size_t)*(const uint16_t *)ebda_ptr * 1024;
    if (!ebda_size) goto skip_ebda;

    for (const char *ptr = ebda_ptr; (uintptr_t)ptr < (uintptr_t)ebda_ptr + ebda_size; ptr += 16) {
        if (is_valid_rsdp((const struct acpi_rsdp *)ptr)) {
            rsdp_addr = (const struct acpi_rsdp *)ptr;
            break;
        }
    }

    if (rsdp_addr) {
        if (out_rsdp) *out_rsdp = rsdp_addr;

        return STATUS_SUCCESS;
    }

skip_ebda:
    for (const char *ptr = (const char *)0x000E0000; (uintptr_t)ptr < 0x00100000; ptr += 16) {
        if (is_valid_rsdp((const struct acpi_rsdp *)ptr)) {
            rsdp_addr = (const struct acpi_rsdp *)ptr;
            break;
        }
    }

    if (rsdp_addr) {
        if (out_rsdp) *out_rsdp = rsdp_addr;

        return STATUS_SUCCESS;
    }

    return STATUS_ENTRY_NOT_FOUND;
}
