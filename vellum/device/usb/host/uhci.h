#ifndef __UHCI_H__
#define __UHCI_H__

#include <stdint.h>

#include <bus/pci/device.h>
#include <bus/usb/device.h>

#define UHCI_IOREG_USBCMD       0x0000
#define UHCI_IOREG_USBSTS       0x0002
#define UHCI_IOREG_USBINTR      0x0004
#define UHCI_IOREG_FRNUM        0x0006
#define UHCI_IOREG_FRBASE       0x0008
#define UHCI_IOREG_SOFMOD       0x000C
#define UHCI_IOREG_PORTSC1      0x0010
#define UHCI_IOREG_PORTSC2      0x0012

#define UHCI_USBCMD_RS          0x0001
#define UHCI_USBCMD_HCRESET     0x0002
#define UHCI_USBCMD_GRESET      0x0004
#define UHCI_USBCMD_EGSM        0x0008
#define UHCI_USBCMD_FGR         0x0010
#define UHCI_USBCMD_SWDBG       0x0020
#define UHCI_USBCMD_CF          0x0040
#define UHCI_USBCMD_MAXP        0x0080

#define UHCI_USBSTS_HCHALTED    0x0020
#define UHCI_USBSTS_HCPE        0x0010
#define UHCI_USBSTS_HSE         0x0008
#define UHCI_USBSTS_RD          0x0004
#define UHCI_USBSTS_USBEI       0x0002
#define UHCI_USBSTS_USBINT      0x0001

#define UHCI_USBINTR_SPIE       0x0008
#define UHCI_USBINTR_IOCE       0x0004
#define UHCI_USBINTR_RIE        0x0002
#define UHCI_USBINTR_TCIE       0x0001

#define UHCI_PORTSC_SUSPEND     0x1000
#define UHCI_PORTSC_PORTRST     0x0200
#define UHCI_PORTSC_LOWSPD      0x0100
#define UHCI_PORTSC_RESUME      0x0040
#define UHCI_PORTSC_LSTATUS     0x0030
#define UHCI_PORTSC_PEDC        0x0008
#define UHCI_PORTSC_PEN         0x0004
#define UHCI_PORTSC_CSC         0x0002
#define UHCI_PORTSC_CCS         0x0001

struct uhci_frame_ptr {
    uint32_t ptr_shr4 : 28;
    uint32_t : 2;
    uint32_t qh_td_sel : 1;
    uint32_t terminate : 1;
} __packed;

struct uhci_transfer_descriptor {
    uint32_t link_ptr_shr4 : 28;
    uint32_t : 1;
    uint32_t db_sel : 1;
    uint32_t qh_td_sel : 1;
    uint32_t terminate : 1;

    uint32_t : 2;
    uint32_t short_packet_detect : 1;
    uint32_t error_limit : 2;  
    uint32_t low_speed : 1;
    uint32_t isochronous_select : 1;
    uint32_t interrupt_on_complete : 1;
    uint32_t status : 8;
    uint32_t : 5;
    uint32_t actual_len : 11;
    
    uint32_t max_len : 11;
    uint32_t : 1;
    uint32_t data_toggle : 1;
    uint32_t endpoint : 4;
    uint32_t device_addr : 7;
    uint32_t packet_id : 8;
    
    uint32_t buffer_ptr;
    uint8_t reserved[16];
} __packed;

struct pci_uhci_controller_device {
    struct pci_device *pci_device;
};

struct uhci_usb_device {
    struct usb_device *usb_device;
};

#endif  // __UHCI_H__
