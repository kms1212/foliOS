#ifndef __VELLUM_DEVICE_H__
#define __VELLUM_DEVICE_H__

#include <stdint.h>

#include <vellum/compiler.h>
#include <vellum/panic.h>
#include <vellum/resource.h>
#include <vellum/status.h>

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
    VlStatus (*probe)(
        struct device **, struct device_driver *, struct device *, struct resource *, int
    );
    VlStatus (*remove)(struct device *);
    VlStatus (*get_interface)(struct device *, const char *, const void **);
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

VlStatus VlDev_Create(struct device **devout, struct device_driver *drv, struct device *parent);
void VlDev_Remove(struct device *dev);

struct device *VlDev_GetFirst(void);
VlStatus VlDev_Find(const char *name, struct device **dev);

VlStatus VlDev_GenerateName(const char *basename, char *buf, size_t len);

VlStatus VlDev_CreateDriver(struct device_driver **drv);

VlStatus VlDev_FindDriver(const char *name, struct device_driver **drv);

#define REGISTER_DEVICE_DRIVER(name, init_func)                                                    \
    __constructor static void _register_driver_##name(void)                                        \
    {                                                                                              \
        init_func();                                                                               \
    }

#endif  // __VELLUM_DEVICE_H__
