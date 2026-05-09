#ifndef __PLAT_PCI_CFGSPACE_H__
#define __PLAT_PCI_CFGSPACE_H__

#include <stdint.h>

uint32_t StPciP_ReadCfg32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

uint16_t StPciP_ReadCfg16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

uint8_t StPciP_ReadCfg8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);

void StPciP_WriteCfg32(
    uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value
);

void StPciP_WriteCfg16(
    uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value
);

void StPciP_WriteCfg8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value);

#endif  // __PLAT_PCI_CFGSPACE_H__
