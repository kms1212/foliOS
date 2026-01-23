// /* Find EHCI Controller */
// int ehci_device_index = -1;
// for (int i = 0; i < pci_count; i++) {
//     if (pci_devices[i].base_class == 0x0C &&
//         pci_devices[i].sub_class == 0x03 && 
//         pci_devices[i].interface == 0x20) {
//         printf("EHCI Controller found at 0x%04X, device 0x%04X, function %d\r\n", pci_devices[i].vendor_id, pci_devices[i].device_id, pci_devices[i].function);

//         ehci_device_index = i;
//     }
// }

// if (ehci_device_index >= 0) {
//     /* Get MMIO Address of EHCI Controller */
//     uint32_t ehci_mmio_address = _bus_pci_cfg_read(
//         pci_devices[ehci_device_index].bus,
//         pci_devices[ehci_device_index].device,
//         pci_devices[ehci_device_index].function,
//         PCI_CFGSPACE_BAR0,
//         sizeof(ehci_mmio_address)) & 0xFFFFFFF0;
//     printf("EHCI MMIO Address: 0x%08lX\r\n", ehci_mmio_address);
// }
