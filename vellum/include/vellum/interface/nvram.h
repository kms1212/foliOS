#ifndef __VELLUM_INTERFACE_NVRAM_H__
#define __VELLUM_INTERFACE_NVRAM_H__

#include <stdint.h>

#include <vellum/device.h>
#include <vellum/status.h>

struct nvram_interface {
    VlStatus (*read_nvram)(struct device *, int, uint8_t *);
    VlStatus (*write_nvram)(struct device *, int, uint8_t);
};

#endif  // __VELLUM_INTERFACE_NVRAM_H__
