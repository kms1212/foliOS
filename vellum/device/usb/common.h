#ifndef __BUS_USB_COMMON_H__
#define __BUS_USB_COMMON_H__

#define USB_PACKET_ID_OUT       0x1
#define USB_PACKET_ID_IN        0x9
#define USB_PACKET_ID_SOF       0x5
#define USB_PACKET_ID_SETUP     0xD
#define USB_PACKET_ID_DATA0     0x3
#define USB_PACKET_ID_DATA1     0xB
#define USB_PACKET_ID_DATA2     0x7
#define USB_PACKET_ID_MDATA     0xF
#define USB_PACKET_ID_ACK       0x2
#define USB_PACKET_ID_NAK       0xA
#define USB_PACKET_ID_STALL     0xE
#define USB_PACKET_ID_NYET      0x6
#define USB_PACKET_ID_PRE       0xC
#define USB_PACKET_ID_ERR       0xC
#define USB_PACKET_ID_SPLIT     0x8
#define USB_PACKET_ID_PING      0x4

#endif // __BUS_USB_COMMON_H__
