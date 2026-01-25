#ifndef __VELLUM_PCI_CFGSPACE_H__
#define __VELLUM_PCI_CFGSPACE_H__

#include <stdint.h>

#define PCI_CFGSPACE_VENDORID          0x00
#define PCI_CFGSPACE_DEVICEID          0x02
#define PCI_CFGSPACE_COMMAND           0x04
#define PCI_CFGSPACE_STATUS            0x06
#define PCI_CFGSPACE_REV_ID            0x08
#define PCI_CFGSPACE_INTERFACE         0x09
#define PCI_CFGSPACE_SUB_CLASS         0x0A
#define PCI_CFGSPACE_BASE_CLASS        0x0B
#define PCI_CFGSPACE_CACHELINE_SIZE    0x0C
#define PCI_CFGSPACE_LATENCY_TIMER     0x0D
#define PCI_CFGSPACE_HEADER_TYPE       0x0E
#define PCI_CFGSPACE_BIST              0x0F
#define PCI_CFGSPACE_BAR0              0x10
#define PCI_CFGSPACE_BAR1              0x14
#define PCI_CFGSPACE_BAR2              0x18
#define PCI_CFGSPACE_BAR3              0x1C
#define PCI_CFGSPACE_BAR4              0x20
#define PCI_CFGSPACE_BAR5              0x24
#define PCI_CFGSPACE_CARDBUS_CIS_PTR   0x28
#define PCI_CFGSPACE_SUBSYS_VENDOR_ID  0x2C
#define PCI_CFGSPACE_SUBSYS_ID         0x2E
#define PCI_CFGSPACE_EXP_ROM_BASE_ADDR 0x30
#define PCI_CFGSPACE_CAPABILITIES_PTR  0x34
#define PCI_CFGSPACE_INTERRUPT_LINE    0x3C
#define PCI_CFGSPACE_INTERRUPT_PIN     0x3D
#define PCI_CFGSPACE_MIN_GNT           0x3E
#define PCI_CFGSPACE_MAX_LATENCY       0x3F

#define PCI_COMMAND_INT_DISABLE       0x0400
#define PCI_COMMAND_FAST_B2B_ENABLE   0x0200
#define PCI_COMMAND_SERR_ENABLE       0x0100
#define PCI_COMMAND_PARITY_ERROR_RESP 0x0040
#define PCI_COMMAND_VGA_PALETTE_SNOOP 0x0020
#define PCI_COMMAND_MEM_WRITE_IVD_ENA 0x0010
#define PCI_COMMAND_SPECIAL_CYCLES    0x0008
#define PCI_COMMAND_BUS_MASTER        0x0004
#define PCI_COMMAND_MEMORY_SPACE      0x0002
#define PCI_COMMAND_IO_SPACE          0x0001

#define PCI_STATUS_PARITY_ERROR_DET  0x8000
#define PCI_STATUS_SYSTEM_ERROR_SIG  0x4000
#define PCI_STATUS_MASTER_ABORT_RECV 0x2000
#define PCI_STATUS_TARGET_ABORT_RECV 0x1000
#define PCI_STATUS_TARGET_ABORT_SIG  0x0800
#define PCI_STATUS_DEVSEL_TIMING     0x0600
#define PCI_STATUS_MASTER_PARITY_ERR 0x0100
#define PCI_STATUS_FAST_B2B_CAPABLE  0x0080
#define PCI_STATUS_66MHZ_CAPABE      0x0020
#define PCI_STATUS_CAPABILITIES_LIST 0x0010
#define PCI_STATUS_INTERRUPT_STATUS  0x0008

union pci_bar_value {
    uint32_t raw;
    struct {
        uint32_t base_addr_shr4 : 28;
        uint32_t prefetchable : 1;
        uint32_t type : 2;
        uint32_t memory_space : 1;
    } mem;
    struct {
        uint32_t base_addr_shr4 : 30;
        uint32_t : 1;
        uint32_t memory_space : 1;
    } io;
};

#endif  // __VELLUM_PCI_CFGSPACE_H__
