#ifndef __VELLUM_ASM_PCI_CFGSPACE_H__
#define __VELLUM_ASM_PCI_CFGSPACE_H__

#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0x0CF8
#define PCI_CONFIG_DATA 0x0CFC

uint32_t _bus_pci_cfg_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

uint16_t _bus_pci_cfg_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

uint8_t _bus_pci_cfg_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

void _bus_pci_cfg_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);

void _bus_pci_cfg_write16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);

void _bus_pci_cfg_write8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value);

#endif // __VELLUM_ASM_PCI_CFGSPACE_H__
