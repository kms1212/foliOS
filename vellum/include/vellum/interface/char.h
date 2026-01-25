#ifndef __VELLUM_INTERFACE_CHAR_H__
#define __VELLUM_INTERFACE_CHAR_H__

#include <wchar.h>

#include <vellum/device.h>
#include <vellum/status.h>

struct char_interface {
    status_t (*seek)(struct device *, off_t, int, off_t *);
    status_t (*read)(struct device *, char *, size_t, size_t *);
    status_t (*write)(struct device *, const char *, size_t, size_t *);
    status_t (*flush)(struct device *);
};

struct wchar_interface {
    status_t (*seek)(struct device *, off_t, int, off_t *);
    status_t (*read)(struct device *, wchar_t *, size_t, size_t *);
    status_t (*write)(struct device *, const wchar_t *, size_t, size_t *);
    status_t (*flush)(struct device *);
};

#endif  // __VELLUM_INTERFACE_CHAR_H__
