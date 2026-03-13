#include <vellum/plat/pci/cfgspace.h>

#include <vellum/arch/io.h>

uint32_t VlPciP_ReadCfg32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t address =
        0x80000000 | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);

    VlA_Out32(PCI_CONFIG_ADDRESS, address);
    return VlA_In32(PCI_CONFIG_DATA);
}

uint16_t VlPciP_ReadCfg16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t val = VlPciP_ReadCfg32(bus, device, function, offset);
    return (val >> ((offset & 0x3) << 3)) & 0xFFFF;
}

uint8_t VlPciP_ReadCfg8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t val = VlPciP_ReadCfg32(bus, device, function, offset);
    return (val >> ((offset & 0x3) << 3)) & 0xFF;
}

void VlPciP_WriteCfg32(
    uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value
)
{
    uint32_t address =
        0x80000000 | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);

    VlA_Out32(PCI_CONFIG_ADDRESS, address);
    VlA_Out32(PCI_CONFIG_DATA, value);
}

void VlPciP_WriteCfg16(
    uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value
)
{
    uint32_t value32 = VlPciP_ReadCfg32(bus, device, function, offset & 0xFC);

    value32 &= ~(0xFFFF << ((offset & 0x3) << 3));
    value32 |= (value & 0xFFFF) << ((offset & 0x3) << 3);
    VlA_Out32(PCI_CONFIG_DATA, value32);

    VlPciP_WriteCfg32(bus, device, function, offset & 0xFC, value32);
}

void VlPciP_WriteCfg8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value)
{
    uint32_t value32 = VlPciP_ReadCfg32(bus, device, function, offset & 0xFC);

    value32 &= ~(0xFF << ((offset & 0x3) << 3));
    value32 |= (value & 0xFF) << ((offset & 0x3) << 3);
    VlA_Out32(PCI_CONFIG_DATA, value32);

    VlPciP_WriteCfg32(bus, device, function, offset & 0xFC, value32);
}
