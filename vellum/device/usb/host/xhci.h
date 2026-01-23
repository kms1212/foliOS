#ifndef __XHCI_H__
#define __XHCI_H__

#include <stdint.h>

struct xhci_capability_registers {
    uint8_t caplength;
    uint8_t reserved;
    uint16_t hci_version;
    uint32_t hcs_params1;
    uint32_t hcs_params2;
    uint32_t hcs_params3;
    uint32_t hcc_params1;
    uint16_t doorbell_offset;
    uint16_t rts_offset;
    uint32_t hcc_params2;
} __packed;

struct xhci_operational_registers {
    uint32_t usb_command;
    uint32_t usb_status;
    uint32_t reserved1[3];
    uint32_t device_notification_control;
    uint64_t command_ring_control;
    uint32_t reserved2[5];
    uint64_t dcbaap;
    uint32_t configure;
} __packed;

struct xhci_port_registers {
    uint32_t port_status;
    uint32_t port_pm_status;
    uint32_t port_link;
    uint32_t reserved;
} __packed;

struct xhci_runtime_registers {
    uint32_t microframe_index;
} __packed;

struct xhci_interrupter_registers {
    uint32_t interrupter_management;
    uint32_t interrupter_moderation;
    uint32_t event_ring_seg_table_size;
    uint32_t reserved;
    uint64_t event_ring_seg_table_base;
    uint64_t event_ring_dequeue_pointer;
} __packed;

#endif  // __XHCI_H__
