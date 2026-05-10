#ifndef __PLAT_PCI_CFGSPACE_H__
#define __PLAT_PCI_CFGSPACE_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/status.h>

StStatus StPciP_ReadCfg32(
    uint8_t bus __in,
    uint8_t device __in,
    uint8_t function __in,
    uint16_t offset __in,
    uint32_t *value __out
);

StStatus StPciP_ReadCfg16(
    uint8_t bus __in,
    uint8_t device __in,
    uint8_t function __in,
    uint16_t offset __in,
    uint16_t *value __out
);

StStatus StPciP_ReadCfg8(
    uint8_t bus __in,
    uint8_t device __in,
    uint8_t function __in,
    uint16_t offset __in,
    uint8_t *value __out
);

StStatus StPciP_WriteCfg32(
    uint8_t bus __in,
    uint8_t device __in,
    uint8_t function __in,
    uint16_t offset __in,
    uint32_t value __in
);

StStatus StPciP_WriteCfg16(
    uint8_t bus __in,
    uint8_t device __in,
    uint8_t function __in,
    uint16_t offset __in,
    uint16_t value __in
);

StStatus StPciP_WriteCfg8(
    uint8_t bus __in,
    uint8_t device __in,
    uint8_t function __in,
    uint16_t offset __in,
    uint8_t value __in
);

#endif  // __PLAT_PCI_CFGSPACE_H__
