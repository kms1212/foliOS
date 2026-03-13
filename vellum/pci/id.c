#include <vellum/pci/id.h>

#include <vellum/pci/driver.h>

int VlPci_MatchId(
    const struct pci_device_driver *driver,
    uint16_t vendor_id,
    uint16_t device_id,
    uint16_t base_class,
    uint16_t sub_class,
    uint16_t interface
)
{
    if (driver->id.vendor_id != PCI_DEVICE_ID_ANY && driver->id.vendor_id != vendor_id) {
        return 0;
    }

    if (driver->id.device_id != PCI_DEVICE_ID_ANY && driver->id.device_id != device_id) {
        return 0;
    }

    if (driver->id.base_class != PCI_DEVICE_ID_ANY && driver->id.base_class != base_class) {
        return 0;
    }

    if (driver->id.sub_class != PCI_DEVICE_ID_ANY && driver->id.sub_class != sub_class) {
        return 0;
    }

    if (driver->id.interface != PCI_DEVICE_ID_ANY && driver->id.interface != interface) {
        return 0;
    }

    return 1;
}
