#ifndef __ATA_H__
#define __ATA_H__

#include <stdint.h>

struct ata_device_ident {
    uint16_t general_config;
    uint16_t obsolete1;
    uint16_t vendor_config;
    uint16_t obsolete2;
    uint16_t retired1[2];
    uint16_t obsolete3;
    uint16_t reserved_compactflash1[2];
    uint16_t retired2;
    char serial_number[20];
    uint16_t retired3[2];
    uint16_t obsolete4;
    char firmware_revision[8];
    char model_number[40];
    uint16_t read_write_multiple_max_sectors;
    uint16_t reserved1;
    uint16_t capabilities1;
    uint16_t capabilities2;
    uint16_t obsolete5[2];
    uint16_t valid_fields;
    uint16_t obsolete6[5];
    uint16_t read_write_multiple_transfered_sectors_per_interrupt;
    uint16_t user_addressable_sectors_count[2];
    uint16_t obsolete7;
    uint16_t supported_selected_mwdma_modes;
    uint16_t supported_pio_modes;
    uint16_t min_mwdma_cycle_time_ns;
    uint16_t rec_mwdma_cycle_time_ns;
    uint16_t min_pio_cycle_time_ns;
    uint16_t min_pio_cycle_iordy_time_ns;
    uint16_t reserved2[6];
    uint16_t queue_depth;
    uint16_t reserved3[4];
    uint16_t supported_major_versions;
    uint16_t supported_minor_versions;
    uint16_t supported_command_sets[3];
    uint16_t enabled_command_sets[2];
    uint16_t default_command_sets;
    uint16_t supported_selected_udma_modes;
    uint16_t security_erase_unit_time_required;
    uint16_t enhanced_security_erase_time_required;
    uint16_t current_advanced_pm_value;
    uint16_t master_password_revision;
    uint16_t hwreset_result;
    uint16_t acoustic_management_value;
    uint16_t stream_min_request_size;
    uint16_t stream_transfer_time;
    uint16_t stream_access_latency;
    uint16_t streaming_performance_granularity[2];
    uint16_t maximum_user_lba_address_lba48[4];
    uint16_t reserved4[23];
    uint16_t removable_media_status_notification_support;
    uint16_t security_status;
    uint16_t vendor_specific[31];
    uint16_t cfa_power_mode_1;
    uint16_t reserved_compactflash2[15];
    uint16_t current_media_serial_number[30];
    uint16_t reserved5[49];
    uint16_t integrity;
};

#endif // __ATA_H__
