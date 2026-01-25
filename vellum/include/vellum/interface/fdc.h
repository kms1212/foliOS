#ifndef __VELLUM_INTERFACE_FLOPPY_H__
#define __VELLUM_INTERFACE_FLOPPY_H__

#include <vellum/device.h>
#include <vellum/status.h>

struct fdc_command {
    uint8_t send_size;
    uint8_t recv_size;
    uint8_t wait_irq;
    uint8_t command;
    uint8_t data[12];
};

struct fdc_interface {
    status_t (*reset)(struct device *, int);
    status_t (*send_command)(struct device *, int, struct fdc_command *);
    status_t (*send_command_dma_input)(struct device *, int, struct fdc_command *, void *, long);
    status_t (*send_command_dma_output)(
        struct device *, int, struct fdc_command *, const void *, long
    );
};

#endif  // __VELLUM_INTERFACE_FLOPPY_H__
