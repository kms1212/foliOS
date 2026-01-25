#ifndef __ATAPI_H__
#define __ATAPI_H__

#include <stdint.h>

#include <vellum/compiler.h>

struct atapi_device_ident {
    uint16_t general_config;
    uint16_t reserved1;
    uint16_t vendor_config;
    uint16_t reserved2[7];
    char serial_number[20];
    uint16_t reserved3[3];
    char firmware_rev[8];
    char model_number[40];
    uint16_t reserved4[2];
    uint16_t capabilities1;
    uint16_t capabilities2;
    uint16_t obsolete1[2];
    uint16_t valid_fields;
    uint16_t reserved5[8];
    uint16_t supported_selected_mwdma_modes;
    uint16_t supported_pio_modes;
    uint16_t min_mwdma_cycle_time_ns;
    uint16_t rec_mwdma_cycle_time_ns;
    uint16_t min_pio_cycle_time_ns;
    uint16_t min_pio_cycle_iordy_time_ns;
    uint16_t reserved6[2];
    uint16_t typical_PACKET_to_release_ns;
    uint16_t typical_SERVICE_to_nBSY_ns;
    uint16_t reserved7[2];
    uint16_t queue_depth;
    uint16_t reserved8[4];
    uint16_t supported_major_versions;
    uint16_t supported_minor_versions;
    uint16_t supported_command_sets[3];
    uint16_t enabled_command_sets[2];
    uint16_t default_command_sets;
    uint16_t supported_selected_udma_modes;
    uint16_t reserved9[4];
    uint16_t hwreset_result;
    uint16_t reserved10[31];
    uint16_t byte_count_0_behavior;
    uint16_t obsolete2;
    uint16_t removable_media_status_notification_support;
    uint16_t security_status;
    uint16_t vendor_specific[31];
    uint16_t reserved_compactflash[26];
    uint16_t reserved11[79];
    uint16_t integrity;
} __packed;

#endif  // __ATAPI_H__
