#ifndef __VELLUM_INTERFACE_HID_H__
#define __VELLUM_INTERFACE_HID_H__

#include <vellum/status.h>
#include <vellum/device.h>

struct hid_interface {
    status_t (*wait_event)(struct device *);
    status_t (*poll_event)(struct device *, uint16_t *, uint16_t *);
};

#endif // __VELLUM_INTERFACE_HID_H__
