#include <bus/pci/scan.h>

#include <asm/pci/cfgspace.h>

#include <bus/pci/device.h>
#include <bus/pci/cfgspace.h>

struct pci_device_list {
    struct pci_device *devices;
    int count;
    int max_count;
};

int _bus_pci_function_scan(struct pci_device_list *list, int bus, int device, int function)
{
    uint16_t vendor_id = _bus_pci_cfg_read16(bus, device, function, PCI_CFGSPACE_VENDORID);
    uint16_t device_id = _bus_pci_cfg_read16(bus, device, function, PCI_CFGSPACE_DEVICEID);
    uint8_t base_class = _bus_pci_cfg_read8(bus, device, function, PCI_CFGSPACE_BASE_CLASS);
    uint8_t sub_class = _bus_pci_cfg_read8(bus, device, function, PCI_CFGSPACE_SUB_CLASS);
    uint8_t interface = _bus_pci_cfg_read8(bus, device, function, PCI_CFGSPACE_INTERFACE);

    if (list->count >= list->max_count) {
        return -1;
    }

    /*
    list->devices[list->count] = (struct pci_device){
        .bus = bus,
        .device = device,
        .function = function,
        .vendor_id = vendor_id,
        .device_id = device_id,
        .base_class = base_class,
        .sub_class = sub_class,
        .interface = interface,
    };
    */

    list->count++;

    return 0;
}

int _bus_pci_device_scan(struct pci_device_list *list, int bus, int device)
{
    int ret;

    ret = _bus_pci_function_scan(list, bus, device, 0);
    if (ret) return ret;

    uint8_t header_type = _bus_pci_cfg_read8(bus, device, 0, PCI_CFGSPACE_HEADER_TYPE);
    if (!(header_type & 0x80)) {
        return 0;
    }

    for (int function = 1; function < 8; function++) {
        uint16_t vendor_id = _bus_pci_cfg_read16(bus, device, function, PCI_CFGSPACE_VENDORID);
        if (vendor_id == 0xFFFF) {
            continue;
        }
        
        ret = _bus_pci_function_scan(list, bus, device, function);
        if (ret) return ret;
    }

    return 0;
}

int _bus_pci_bus_scan(struct pci_device_list *list, int bus)
{
    int ret;

    for (int device = 0; device < 32; device++) {
        uint16_t vendor_id = _bus_pci_cfg_read16(bus, device, 0, PCI_CFGSPACE_VENDORID);
        if (vendor_id == 0xFFFF) {
            continue;
        }
        
        ret = _bus_pci_device_scan(list, bus, device);
        if (ret) return ret;
    }

    return 0;
}

int pci_host_scan(struct pci_device *buf, int max_count)
{
    int ret;
    struct pci_device_list list;
    list.devices = buf;
    list.count = 0;
    list.max_count = max_count;

    uint8_t header_type = _bus_pci_cfg_read8(0, 0, 0, PCI_CFGSPACE_HEADER_TYPE);

    if (!(header_type & 0x80)) {
        ret = _bus_pci_bus_scan(&list, 0);
        if (ret) return ret;
        return list.count;
    }

    for (int function = 1; function < 8; function++) {
        uint16_t vendor_id = _bus_pci_cfg_read16(0, 0, 0, PCI_CFGSPACE_VENDORID);
        if (vendor_id == 0xFFFF) {
            continue;
        }

        ret = _bus_pci_function_scan(&list, 0, 0, function);
        if (ret) return ret;
    }

    return list.count;
}
