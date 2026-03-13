#ifndef __VELLUM_ASM_PCI_CFGSPACE_H__
#define __VELLUM_ASM_PCI_CFGSPACE_H__

#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0x0CF8
#define PCI_CONFIG_DATA    0x0CFC

uint32_t VlPciP_ReadCfg32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

uint16_t VlPciP_ReadCfg16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

uint8_t VlPciP_ReadCfg8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

void VlPciP_WriteCfg32(
    uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value
);

void VlPciP_WriteCfg16(
    uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value
);

void VlPciP_WriteCfg8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value);

#endif  // __VELLUM_ASM_PCI_CFGSPACE_H__
