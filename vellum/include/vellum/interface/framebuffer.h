#ifndef __VELLUM_INTERFACE_FRAMEBUFFER_H__
#define __VELLUM_INTERFACE_FRAMEBUFFER_H__

#include <vellum/device.h>
#include <vellum/status.h>

struct framebuffer_interface {
    VlStatus (*get_framebuffer)(struct device *, void **);
    VlStatus (*invalidate)(struct device *, int, int, int, int);
    VlStatus (*flush)(struct device *);
};

#endif  // __VELLUM_INTERFACE_FRAMEBUFFER_H__
