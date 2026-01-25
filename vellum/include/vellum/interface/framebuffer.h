#ifndef __VELLUM_INTERFACE_FRAMEBUFFER_H__
#define __VELLUM_INTERFACE_FRAMEBUFFER_H__

#include <vellum/device.h>
#include <vellum/status.h>

struct framebuffer_interface {
    status_t (*get_framebuffer)(struct device *, void **);
    status_t (*invalidate)(struct device *, int, int, int, int);
    status_t (*flush)(struct device *);
};

#endif  // __VELLUM_INTERFACE_FRAMEBUFFER_H__
