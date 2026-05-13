#ifndef __VELLUM_INTERFACE_CHAR_H__
#define __VELLUM_INTERFACE_CHAR_H__

#include <wchar.h>

#include <vellum/device.h>
#include <vellum/status.h>

struct char_interface {
    VlStatus (*seek)(struct device *, off_t, int, off_t *);
    VlStatus (*read)(struct device *, char *, size_t, size_t *);
    VlStatus (*write)(struct device *, const char *, size_t, size_t *);
    VlStatus (*flush)(struct device *);
};

struct wchar_interface {
    VlStatus (*seek)(struct device *, off_t, int, off_t *);
    VlStatus (*read)(struct device *, wchar_t *, size_t, size_t *);
    VlStatus (*write)(struct device *, const wchar_t *, size_t, size_t *);
    VlStatus (*flush)(struct device *);
};

#endif  // __VELLUM_INTERFACE_CHAR_H__
