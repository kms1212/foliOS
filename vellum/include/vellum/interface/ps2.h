#ifndef __VELLUM_INTERFACE_PS2_H__
#define __VELLUM_INTERFACE_PS2_H__

#include <vellum/device.h>
#include <vellum/status.h>

struct ps2_interface {
    status_t (*enable_port)(struct device *, int);
    status_t (*disable_port)(struct device *, int);
    status_t (*test_port)(struct device *, int);
    status_t (*send_data)(struct device *, int, const uint8_t *, int);
    status_t (*recv_data)(struct device *, int, uint8_t *, int);
    status_t (*irq_get_byte)(struct device *, uint8_t *);
};

#endif  // __VELLUM_INTERFACE_PS2_H__
