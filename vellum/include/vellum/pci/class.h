#ifndef __VELLUM_PCI_CLASS_H__
#define __VELLUM_PCI_CLASS_H__

#include <stdint.h>

const char *VlPci_GetDeviceClassName(uint8_t class);
const char *VlPci_GetDeviceSubclassName(uint8_t class, uint8_t subclass);
const char *VlPci_GetDeviceInterfaceName(uint8_t class, uint8_t subclass, uint8_t interface);

#endif  // __VELLUM_PCI_CLASS_H__
