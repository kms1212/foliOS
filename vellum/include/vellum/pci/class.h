#ifndef __VELLUM_PCI_CLASS_H__
#define __VELLUM_PCI_CLASS_H__

#include <stdint.h>

const char *_bus_pci_device_get_class_name(uint8_t class);
const char *_bus_pci_device_get_subclass_name(uint8_t class, uint8_t subclass);
const char *_bus_pci_device_get_interface_name(uint8_t class, uint8_t subclass, uint8_t interface);

#endif  // __VELLUM_PCI_CLASS_H__
