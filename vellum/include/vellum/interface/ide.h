#ifndef __VELLUM_INTERFACE_IDE_H__
#define __VELLUM_INTERFACE_IDE_H__

#include <vellum/device.h>
#include <vellum/status.h>

struct ata_command {
    uint8_t extended;
    uint8_t command;
    uint16_t features;
    uint16_t count;
    uint16_t sector;
    uint16_t cylinder_low;
    uint16_t cylinder_high;
    uint8_t drive_head;
};

struct ide_interface {
    status_t (*send_command)(struct device *, struct ata_command *);
    status_t (*send_command_pVlA_Input)(
        struct device *, struct ata_command *, void *, size_t, size_t, size_t *
    );
    status_t (*send_command_pVlA_Output)(
        struct device *, struct ata_command *, const void *, size_t, size_t, size_t *
    );
    status_t (*send_command_packet)(struct device *, int, const uint8_t *, size_t, size_t);
    status_t (*send_command_packet_input)(
        struct device *, int, const uint8_t *, size_t, void *, size_t, size_t, size_t *
    );
    status_t (*send_command_packet_output)(
        struct device *, int, const uint8_t *, size_t, const void *, size_t, size_t, size_t *
    );
};

#endif  // __VELLUM_INTERFACE_IDE_H__
