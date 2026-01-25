#include <vellum/shell.h>

static int lspci_handler(struct shell_instance *inst, int argc, char **argv)
{
    /*
    struct pci_device pci_devices[32];
    int pci_count = pci_host_scan(pci_devices, ARRAY_SIZE(pci_devices));

    for (int i = 0; i < pci_count; i++) {
        uint8_t bus = pci_devices[i].bus;
        uint8_t device = pci_devices[i].device;
        uint8_t function = pci_devices[i].function;
        printf("bus %d device %d function %d: vendor 0x%04X, device 0x%04X (%d, %d, %d)\n",
            bus, device, function,
            pci_devices[i].vendor_id, pci_devices[i].device_id,
            pci_devices[i].base_class, pci_devices[i].sub_class, pci_devices[i].interface
        );
        printf("%s -> %s -> %s\n",
            _bus_pci_device_get_class_name(pci_devices[i].base_class),
            _bus_pci_device_get_subclass_name(pci_devices[i].base_class, pci_devices[i].sub_class),
            _bus_pci_device_get_interface_name(pci_devices[i].base_class, pci_devices[i].sub_class,
    pci_devices[i].interface)
        );
        uint32_t bar = _bus_pci_cfg_read32(bus, device, function, PCI_CFGSPACE_BAR0);
        if (bar) {
            printf("\tBAR0: %08lX\n", bar);
        }
        bar = _bus_pci_cfg_read32(bus, device, function, PCI_CFGSPACE_BAR1);
        if (bar) {
            printf("\tBAR1: %08lX\n", bar);
        }
        bar = _bus_pci_cfg_read32(bus, device, function, PCI_CFGSPACE_BAR2);
        if (bar) {
            printf("\tBAR2: %08lX\n", bar);
        }
        bar = _bus_pci_cfg_read32(bus, device, function, PCI_CFGSPACE_BAR3);
        if (bar) {
            printf("\tBAR3: %08lX\n", bar);
        }
        bar = _bus_pci_cfg_read32(bus, device, function, PCI_CFGSPACE_BAR4);
        if (bar) {
            printf("\tBAR4: %08lX\n", bar);
        }
        bar = _bus_pci_cfg_read32(bus, device, function, PCI_CFGSPACE_BAR5);
        if (bar) {
            printf("\tBAR5: %08lX\n", bar);
        }
    }
        */

    return 0;
}

static struct command lspci_command = {
    .name = "lspci",
    .handler = lspci_handler,
    .help_message = "List PCI devices",
};

static void lspci_command_init(void)
{
    shell_command_register(&lspci_command);
}

REGISTER_SHELL_COMMAND(lspci, lspci_command_init)
