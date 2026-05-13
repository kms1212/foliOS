#ifndef __VELLUM_ACPI_H__
#define __VELLUM_ACPI_H__

#include <stdint.h>

#include <vellum/compiler.h>
#include <vellum/status.h>

struct acpi_rsdp {
    char signature[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t revision;
    uint32_t rsdt_addr;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __packed;

VlStatus VlAcpi_FindRsdp(const struct acpi_rsdp **rsdp);

#endif  // __VELLUM_ACPI_H__
