#ifndef __VELLUM_INTERFACE_HID_H__
#define __VELLUM_INTERFACE_HID_H__

#include <vellum/device.h>
#include <vellum/status.h>

struct hid_interface {
    VlStatus (*wait_event)(struct device *);
    VlStatus (*poll_event)(struct device *, uint16_t *, uint16_t *);
};

#endif  // __VELLUM_INTERFACE_HID_H__
