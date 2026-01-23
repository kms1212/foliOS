#ifndef __EMOS_INTERFACE_NVRAM_H__
#define __EMOS_INTERFACE_NVRAM_H__

#include <vellum/status.h>
#include <vellum/device.h>

struct nvram_interface {
    status_t (*read_nvram)(struct device *, int, uint8_t *);
    status_t (*write_nvram)(struct device *, int, uint8_t );
};

#endif // __EMOS_INTERFACE_NVRAM_H__
