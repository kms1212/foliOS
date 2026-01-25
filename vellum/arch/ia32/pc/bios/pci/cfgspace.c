#include <vellum/asm/pci/cfgspace.h>

#include <vellum/asm/io.h>

uint32_t _bus_pci_cfg_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t address =
        0x80000000 | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);

    io_out32(PCI_CONFIG_ADDRESS, address);
    return io_in32(PCI_CONFIG_DATA);
}

uint16_t _bus_pci_cfg_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t val = _bus_pci_cfg_read32(bus, device, function, offset);
    return (val >> ((offset & 0x3) << 3)) & 0xFFFF;
}

uint8_t _bus_pci_cfg_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t val = _bus_pci_cfg_read32(bus, device, function, offset);
    return (val >> ((offset & 0x3) << 3)) & 0xFF;
}

void _bus_pci_cfg_write32(
    uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value
)
{
    uint32_t address =
        0x80000000 | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);

    io_out32(PCI_CONFIG_ADDRESS, address);
    io_out32(PCI_CONFIG_DATA, value);
}

void _bus_pci_cfg_write16(
    uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value
)
{
    uint32_t value32 = _bus_pci_cfg_read32(bus, device, function, offset & 0xFC);

    value32 &= ~(0xFFFF << ((offset & 0x3) << 3));
    value32 |= (value & 0xFFFF) << ((offset & 0x3) << 3);
    io_out32(PCI_CONFIG_DATA, value32);

    _bus_pci_cfg_write32(bus, device, function, offset & 0xFC, value32);
}

void _bus_pci_cfg_write8(
    uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value
)
{
    uint32_t value32 = _bus_pci_cfg_read32(bus, device, function, offset & 0xFC);

    value32 &= ~(0xFF << ((offset & 0x3) << 3));
    value32 |= (value & 0xFF) << ((offset & 0x3) << 3);
    io_out32(PCI_CONFIG_DATA, value32);

    _bus_pci_cfg_write32(bus, device, function, offset & 0xFC, value32);
}
