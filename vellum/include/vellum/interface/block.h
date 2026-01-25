#ifndef __VELLUM_INTERFACE_BLOCK_H__
#define __VELLUM_INTERFACE_BLOCK_H__

#include <vellum/device.h>
#include <vellum/disk.h>
#include <vellum/status.h>

#define BLOCK_INTERFACE_ID "block"

struct block_interface {
    status_t (*get_block_size)(struct device *, size_t *);
    status_t (*read)(struct device *, lba_t, void *, size_t, size_t *);
    status_t (*write)(struct device *, lba_t, const void *, size_t, size_t *);
};

#endif  // __VELLUM_INTERFACE_BLOCK_H__
