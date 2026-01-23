static int a = 0;

/*
__aligned(4096)
struct uhci_frame_ptr uhci_frame_list[1024];

__aligned(4)
struct uhci_transfer_descriptor uhci_td_list[1024];

uint8_t uhci_packet_buf[1024];

static void uhci_schedule_transfer(int low_speed, int endpoint, int device_addr, int packet_id, void *buf, long len)
{
    packet_id &= 0x0F;
    packet_id |= (~packet_id & 0x0F) << 4;

    uhci_td_list[0] = (struct uhci_transfer_descriptor) {
        .link_ptr_shr4 = 0,
        .db_sel = 0,
        .qh_td_sel = 0,
        .terminate = 1,
        .short_packet_detect = 0,
        .error_limit = 0,
        .low_speed = low_speed,
        .isochronous_select = 0,
        .interrupt_on_complete = 0,
        .status = 0,
        .actual_len = 0,
        .max_len = len,
        .data_toggle = 0,
        .endpoint = endpoint,
        .device_addr = device_addr,
        .packet_id = packet_id,
        .buffer_ptr = (uint32_t)buf,
    };

    uhci_frame_list[0] = (struct uhci_frame_ptr) {
        .ptr_shr4 = (uint32_t)&uhci_td_list[0] >> 4,
        .qh_td_sel = 0,
        .terminate = 0,
    };
}
*/

// /* Find UHCI Controller */
// int uhci_device_index = -1;
// for (int i = 0; i < pci_count; i++) {
//     if (pci_devices[i].base_class == 0x0C &&
//         pci_devices[i].sub_class == 0x03 && 
//         pci_devices[i].interface == 0x00) {
//         printf("UHCI Controller found at 0x%04X, device 0x%04X, function %d\r\n", pci_devices[i].vendor_id, pci_devices[i].device_id, pci_devices[i].function);

//         uhci_device_index = i;
//     }
// }
// struct pci_device *uhci_device = &pci_devices[uhci_device_index];

// uint16_t uhci_pmio_address;
// if (uhci_device_index >= 0) {
//     /* Get PMIO Address of UHCI Controller */
//     uhci_pmio_address = _bus_pci_cfg_read(
//         pci_devices[uhci_device_index].bus,
//         pci_devices[uhci_device_index].device,
//         pci_devices[uhci_device_index].function,
//         PCI_CFGSPACE_BAR4,
//         sizeof(uhci_pmio_address)) & 0xFFFFFFFC;
//     printf("UHCI I/O Port Address: 0x%08X\r\n", uhci_pmio_address);
// }

    // /* initialize bus */
    // uint16_t uhci_pcicfgspace_command = _bus_pci_cfg_read(uhci_device->bus, uhci_device->device, uhci_device->function, PCI_CFGSPACE_COMMAND, sizeof(uhci_pcicfgspace_command));
    // _bus_pci_cfg_write(uhci_device->bus, uhci_device->device, uhci_device->function, PCI_CFGSPACE_COMMAND, uhci_pcicfgspace_command | 0x0007, sizeof(uhci_pcicfgspace_command));

    // /* Reset host controller */
    // io_out16(uhci_pmio_address + UHCI_IOREG_USBCMD, 0x0006);

    // /* Wait and clear GRESET */
    // for (int i = 0; i < (1 << 20); i++) {}
    // io_out16(uhci_pmio_address + UHCI_IOREG_USBCMD, 0x0000);

    // /* Wait for host controller reset */
    // while (io_in16(uhci_pmio_address + UHCI_IOREG_USBCMD) & 0x20) {}

    // /* Initialize frame list */
    // for (int i = 0; i < sizeof(uhci_frame_list) / sizeof(uhci_frame_list[0]); i++) {
    //     uhci_frame_list[i] = (struct uhci_frame_ptr){ .ptr_shr4 = 0, .qh_td_sel = 0, .terminate = 1 };
    // }

    // /* Set Values */
    // io_out16(uhci_pmio_address + UHCI_IOREG_USBINTR, 0x000F);
    // io_out16(uhci_pmio_address + UHCI_IOREG_FRNUM, 0x0000);
    // io_out8(uhci_pmio_address + UHCI_IOREG_SOFMOD, 0x40);
    // io_out32(uhci_pmio_address + UHCI_IOREG_FRBASE, (uint32_t)uhci_frame_list);

    // /* init status flags */
    // io_out16(uhci_pmio_address + UHCI_IOREG_USBSTS, 0xFFFF);

    // /* Start host controller */
    // io_out16(uhci_pmio_address + UHCI_IOREG_USBCMD, 0x0081);

    // /* Scan, enable, and reset controller ports */
    // for (int i = 0; i < 2; i++) {
    //     uint16_t portsc =  io_in16(uhci_pmio_address + (i ? UHCI_IOREG_PORTSC1 : UHCI_IOREG_PORTSC2));
    //     if (portsc & 0x0001) {
    //         /* reset port */
    //         portsc |= 0x0200;
    //         io_out16(uhci_pmio_address + (i ? UHCI_IOREG_PORTSC1 : UHCI_IOREG_PORTSC2), portsc);
    //         for (int i = 0; i < 1048576; i++) {}
    //         portsc &= ~0x0200;
    //         io_out16(uhci_pmio_address + (i ? UHCI_IOREG_PORTSC1 : UHCI_IOREG_PORTSC2), portsc);
    //         portsc =  io_in16(uhci_pmio_address + (i ? UHCI_IOREG_PORTSC1 : UHCI_IOREG_PORTSC2));

    //         /* enable port */
    //         portsc |= 0x0004;
    //         io_out16(uhci_pmio_address + (i ? UHCI_IOREG_PORTSC1 : UHCI_IOREG_PORTSC2), portsc);
    //         portsc =  io_in16(uhci_pmio_address + (i ? UHCI_IOREG_PORTSC1 : UHCI_IOREG_PORTSC2));
    //     }

    //     printf("\tPort #%d: connected=%s, enabled=%s, speed=%s\r\n", i, (portsc & 0x0001) ? "true" : "false", (portsc & 0x0004) ? "true" : "false", (portsc & 0x0100) ? "low" : "full");
    // }

    // printf("USBCMD: 0x%04X\r\n", io_in16(uhci_pmio_address + UHCI_IOREG_USBCMD));
    // printf("USBSTS: 0x%04X\r\n", io_in16(uhci_pmio_address + UHCI_IOREG_USBSTS));
    // printf("USBINTR: 0x%04X\r\n", io_in16(uhci_pmio_address + UHCI_IOREG_USBINTR));
    // printf("FRNUM: 0x%04X\r\n", io_in16(uhci_pmio_address + UHCI_IOREG_FRNUM));
    // printf("FRBASE: 0x%08lX\r\n", io_in32(uhci_pmio_address + UHCI_IOREG_FRBASE));
    // printf("SOFMOD: 0x%02X\r\n", io_in8(uhci_pmio_address + UHCI_IOREG_SOFMOD));
    // printf("PORTSC1: 0x%04X\r\n", io_in16(uhci_pmio_address + UHCI_IOREG_PORTSC1));
    // printf("PORTSC2: 0x%04X\r\n", io_in16(uhci_pmio_address + UHCI_IOREG_PORTSC2));
