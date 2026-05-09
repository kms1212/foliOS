#include <plat/pci/cfgspace.h>

#include <stdint.h>

#include <strata/arch/intrinsics/io.h>
#include <strata/handle.h>
#include <strata/log.h>
#include <strata/macros.h>
#include <strata/mm.h>
#include <strata/status.h>

#include "mcfg.server-client.h"

#define MODULE_NAME "pci"

#define PCI_CONFIG_ADDRESS 0x0CF8
#define PCI_CONFIG_DATA    0x0CFC

#define PCI_ECAM_MAX_ENTRIES    16
#define PCI_ECAM_BUS_SIZE       0x100000ULL
#define PCI_ECAM_DEVICE_SIZE    0x8000ULL
#define PCI_ECAM_FUNCTION_SIZE  0x1000ULL
#define PCI_ECAM_FUNCTION_COUNT 8
#define PCI_ECAM_DEVICE_COUNT   32

struct pci_ecam_entry {
    uintptr_t virt_base;
    uintptr_t phys_base;
    uint16_t segment;
    uint8_t start_bus;
    uint8_t end_bus;
};

static struct pci_ecam_entry ecam_entries[PCI_ECAM_MAX_ENTRIES];
static uint32_t ecam_entry_count;
static int cfgspace_initialized;
static int ecam_enabled;

static uint32_t read_legacy_cfg32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t address =
        0x80000000 | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);

    StIoA_Out32(PCI_CONFIG_ADDRESS, address);
    return StIoA_In32(PCI_CONFIG_DATA);
}

static void write_legacy_cfg32(
    uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value
)
{
    uint32_t address =
        0x80000000 | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);

    StIoA_Out32(PCI_CONFIG_ADDRESS, address);
    StIoA_Out32(PCI_CONFIG_DATA, value);
}

static const struct pci_ecam_entry *find_ecam_entry(uint8_t bus)
{
    for (uint32_t index = 0; index < ecam_entry_count; index++) {
        const struct pci_ecam_entry *entry = &ecam_entries[index];

        if (bus >= entry->start_bus && bus <= entry->end_bus) return entry;
    }

    return NULL;
}

static uintptr_t ecam_cfg_addr(
    const struct pci_ecam_entry *entry,
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint16_t offset
)
{
    return entry->virt_base + (((uintptr_t)bus - entry->start_bus) * PCI_ECAM_BUS_SIZE) +
        ((uintptr_t)device * PCI_ECAM_DEVICE_SIZE) +
        ((uintptr_t)function * PCI_ECAM_FUNCTION_SIZE) + offset;
}

static StStatus map_mcfg_entry(const StIfAcpiTblMcfg_Entry *mcfg_entry)
{
    StStatus status;
    St_VirtPage vpn;
    uintptr_t phys_base = (uintptr_t)mcfg_entry->base_address;
    uintptr_t phys_page_base = phys_base & ~(uintptr_t)(PAGE_SIZE - 1);
    uintptr_t phys_page_offset = phys_base - phys_page_base;
    uintptr_t bus_count = (uintptr_t)mcfg_entry->end_bus - mcfg_entry->start_bus + 1;
    uintptr_t map_size = phys_page_offset + bus_count * PCI_ECAM_BUS_SIZE;
    St_PageCount map_page_count = (St_PageCount)ALIGN_DIV(map_size, PAGE_SIZE);

    status = StMm_MapGlobal(
        VMM_DOMAIN_IO,
        &vpn,
        ADDR_TO_FRAME(phys_page_base),
        map_page_count,
        NULL,
        (struct StMm_CompoundFlags){AF_DEFAULT, MF_KERNEL_DEFAULT | MF_NO_CACHE}
    );
    if (!CHECK_SUCCESS(status)) return status;

    ecam_entries[ecam_entry_count++] = (struct pci_ecam_entry){
        .virt_base = PAGE_TO_ADDR(vpn) + phys_page_offset,
        .phys_base = phys_base,
        .segment = mcfg_entry->pci_segment_group,
        .start_bus = mcfg_entry->start_bus,
        .end_bus = mcfg_entry->end_bus,
    };

    return STATUS_SUCCESS;
}

static StStatus init_ecam_from_mcfg(void)
{
    StStatus status;
    StStatus close_status;
    StHandle mcfg_handle;
    uint32_t funcid_base;
    uint32_t result_abiver;
    uint32_t entry_count;
    int handle_opened = 0;

    status =
        StIfAcpiTblMcfg_Open((const uint8_t *)"/System/Firmware/ACPI/Tables/MCFG", 0, &mcfg_handle);
    if (status == STATUS_ENTRY_NOT_FOUND || status == STATUS_NOT_A_DIRECTORY) {
        LOG_DEBUG(LM_CAT_PCI, "ACPI MCFG table not found; using legacy config access\n");
        return STATUS_NOT_SUPPORTED;
    }
    if (!CHECK_SUCCESS(status)) return status;
    handle_opened = 1;

    status = StIfAcpiTblMcfg_Query(mcfg_handle, 0, &funcid_base, &result_abiver);
    if (!CHECK_SUCCESS(status)) goto done;

    status = StIfAcpiTblMcfg_GetEntryCount(mcfg_handle, funcid_base, &entry_count);
    if (!CHECK_SUCCESS(status)) goto done;

    LOG_DEBUG(LM_CAT_PCI, "ACPI MCFG entry count: %u\n", entry_count);

    for (uint32_t index = 0; index < entry_count; index++) {
        StIfAcpiTblMcfg_Entry entry;

        status = StIfAcpiTblMcfg_GetEntry(mcfg_handle, funcid_base, index, &entry);
        if (!CHECK_SUCCESS(status)) goto done;

        LOG_DEBUG(
            LM_CAT_PCI,
            "ACPI MCFG[%u]: base=%016llX segment=%u bus=%u-%u\n",
            index,
            (unsigned long long)entry.base_address,
            entry.pci_segment_group,
            entry.start_bus,
            entry.end_bus
        );

        if (entry.end_bus < entry.start_bus) {
            LOG_WARN(LM_CAT_PCI, "ignoring invalid MCFG[%u] bus range\n", index);
            continue;
        }

        if (entry.pci_segment_group != 0) {
            LOG_DEBUG(LM_CAT_PCI, "ignoring non-zero PCI segment %u\n", entry.pci_segment_group);
            continue;
        }

        if (ecam_entry_count >= ARRAY_SIZE(ecam_entries)) {
            LOG_WARN(LM_CAT_PCI, "too many MCFG entries; ignoring MCFG[%u]\n", index);
            continue;
        }

        status = map_mcfg_entry(&entry);
        if (!CHECK_SUCCESS(status)) {
            LOG_WARN(LM_CAT_PCI, "failed to map MCFG[%u] (status=%08X)\n", index, status);
            status = STATUS_SUCCESS;
        }
    }

    if (ecam_entry_count == 0) status = STATUS_NOT_SUPPORTED;

done:
    if (handle_opened) {
        close_status = StHandle_Close(mcfg_handle);
        if (CHECK_SUCCESS(status) && !CHECK_SUCCESS(close_status)) status = close_status;
    }

    return status;
}

static void ensure_cfgspace_initialized(void)
{
    StStatus status;

    if (cfgspace_initialized) return;
    cfgspace_initialized = 1;

    status = init_ecam_from_mcfg();
    if (!CHECK_SUCCESS(status)) {
        LOG_DEBUG(LM_CAT_PCI, "using legacy PCI config access\n");
        return;
    }

    ecam_enabled = 1;
    LOG_INFO(LM_CAT_PCI, "using PCI ECAM config access\n");
}

uint32_t StPciP_ReadCfg32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    const struct pci_ecam_entry *entry;

    ensure_cfgspace_initialized();

    if (device >= PCI_ECAM_DEVICE_COUNT || function >= PCI_ECAM_FUNCTION_COUNT) {
        return UINT32_MAX;
    }

    entry = ecam_enabled ? find_ecam_entry(bus) : NULL;
    if (entry) {
        uintptr_t addr = ecam_cfg_addr(entry, bus, device, function, offset & 0xFC);
        return *(volatile uint32_t *)addr;
    }

    return read_legacy_cfg32(bus, device, function, offset);
}

uint16_t StPciP_ReadCfg16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t val = StPciP_ReadCfg32(bus, device, function, offset);
    return (val >> ((offset & 0x3) << 3)) & 0xFFFF;
}

uint8_t StPciP_ReadCfg8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t val = StPciP_ReadCfg32(bus, device, function, offset);
    return (val >> ((offset & 0x3) << 3)) & 0xFF;
}

void StPciP_WriteCfg32(
    uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value
)
{
    const struct pci_ecam_entry *entry;

    ensure_cfgspace_initialized();

    if (device >= PCI_ECAM_DEVICE_COUNT || function >= PCI_ECAM_FUNCTION_COUNT) return;

    entry = ecam_enabled ? find_ecam_entry(bus) : NULL;
    if (entry) {
        uintptr_t addr = ecam_cfg_addr(entry, bus, device, function, offset & 0xFC);
        *(volatile uint32_t *)addr = value;
        return;
    }

    write_legacy_cfg32(bus, device, function, offset, value);
}

void StPciP_WriteCfg16(
    uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value
)
{
    uint32_t value32 = StPciP_ReadCfg32(bus, device, function, offset & 0xFC);

    value32 &= ~(0xFFFF << ((offset & 0x3) << 3));
    value32 |= (value & 0xFFFF) << ((offset & 0x3) << 3);

    StPciP_WriteCfg32(bus, device, function, offset & 0xFC, value32);
}

void StPciP_WriteCfg8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value)
{
    uint32_t value32 = StPciP_ReadCfg32(bus, device, function, offset & 0xFC);

    value32 &= ~(0xFF << ((offset & 0x3) << 3));
    value32 |= (value & 0xFF) << ((offset & 0x3) << 3);

    StPciP_WriteCfg32(bus, device, function, offset & 0xFC, value32);
}
