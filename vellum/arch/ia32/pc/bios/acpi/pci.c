#include <uacpi/kernel_api.h>

#include <stdlib.h>

#include <vellum/asm/pci/cfgspace.h>

struct pci_device {
    uacpi_pci_address addr;
};

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle *out_handle)
{
    struct pci_device *pdev = malloc(sizeof(*pdev));
    if (!pdev) {
        return UACPI_STATUS_INTERNAL_ERROR;
    }

    pdev->addr = address;

    *out_handle = pdev;

    return 0;
}

void uacpi_kernel_pci_device_close(uacpi_handle device)
{
    free(device);
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle device, uacpi_size offset, uacpi_u8 *value)
{
    struct pci_device *pdev = device;

    *value = _bus_pci_cfg_read8(pdev->addr.bus, pdev->addr.device, pdev->addr.function, offset);

    return 0;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle device, uacpi_size offset, uacpi_u16 *value)
{
    struct pci_device *pdev = device;

    *value = _bus_pci_cfg_read16(pdev->addr.bus, pdev->addr.device, pdev->addr.function, offset);

    return 0;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle device, uacpi_size offset, uacpi_u32 *value)
{
    struct pci_device *pdev = device;

    *value = _bus_pci_cfg_read32(pdev->addr.bus, pdev->addr.device, pdev->addr.function, offset);

    return 0;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle device, uacpi_size offset, uacpi_u8 value)
{
    struct pci_device *pdev = device;

    _bus_pci_cfg_write8(pdev->addr.bus, pdev->addr.device, pdev->addr.function, offset, value);

    return 0;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle device, uacpi_size offset, uacpi_u16 value)
{
    struct pci_device *pdev = device;

    _bus_pci_cfg_write16(pdev->addr.bus, pdev->addr.device, pdev->addr.function, offset, value);

    return 0;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle device, uacpi_size offset, uacpi_u32 value)
{
    struct pci_device *pdev = device;

    _bus_pci_cfg_write32(pdev->addr.bus, pdev->addr.device, pdev->addr.function, offset, value);

    return 0;
}
