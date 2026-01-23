#ifndef __VELLUM_DEVICE_H__
#define __VELLUM_DEVICE_H__

#include <stdint.h>

#include <vellum/status.h>
#include <vellum/panic.h>
#include <vellum/resource.h>
#include <vellum/compiler.h>

#define DEVICE_NAME_MAX 64

enum device_id_type {
    DIT_STRING = 0,
    DIT_PCI,
    DIT_USB,
};

struct device_id {
    enum device_id_type type;
    union {
        const char *string;
        struct {
            uint16_t vendor, device;
        } pci;
        struct {
            uint16_t vid, pid;
        } usb;
    };
};

struct device;

struct device_driver {
    struct device_driver *next;

    const char *name;
    status_t (*probe)(struct device **, struct device_driver *, struct device *, struct resource *, int);
    status_t (*remove)(struct device *);
    status_t (*get_interface)(struct device *, const char *, const void **);
};

struct device {
    struct device *next;
    struct device *sibling;
    struct device *parent;
    struct device *first_child;

    char name[DEVICE_NAME_MAX];

    struct device_driver *driver;

    void *data;
};

status_t device_create(struct device **devout, struct device_driver *drv, struct device *parent);
void device_remove(struct device *dev);

struct device *device_get_first_dev(void);
status_t device_find(const char *name, struct device **dev);

status_t device_generate_name(const char *basename, char *buf, size_t len);

status_t device_driver_create(struct device_driver **drv);

status_t device_driver_find(const char *name, struct device_driver **drv);

#define REGISTER_DEVICE_DRIVER(name, init_func) \
    __constructor \
    static void _register_driver_##name(void) \
    { \
        init_func(); \
    }

#endif // __VELLUM_DEVICE_H__
