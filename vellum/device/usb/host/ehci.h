#ifndef __EHCI_H__
#define __EHCI_H__

#include <stdint.h>

struct ehci_capability_registers {
    uint8_t caplength;
    uint8_t reserved;
    uint16_t hci_version;
    uint32_t hcs_params;
    uint32_t hcc_params;
    uint32_t hcsp_portroute;
} __packed;

struct ehci_operational_registers {
    uint32_t usb_command;
    uint32_t usb_status;
    uint32_t usb_interrupt_enable;
    uint32_t frame_index;
    uint32_t ctrl_ds_segment;
    uint32_t frame_list_base_addr;
    uint32_t next_async_list_addr;
    uint8_t reserved[36];
    uint32_t config_flag;
    uint32_t port_status[];
} __packed;

#endif // __EHCI_H__
