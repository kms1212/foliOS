#ifndef __VELLUM_ASM_BIOS_MISC_H__
#define __VELLUM_ASM_BIOS_MISC_H__

#include <stdint.h>

#include <vellum/compiler.h>
#include <vellum/status.h>

status_t _pc_bios_check_apm_version(uint16_t *version);

status_t _pc_bios_connect_apm_interface(uint16_t device, void (*entry_point)(void), uint16_t code32_seg, uint16_t code32_seg_len, uint16_t code16_seg, uint16_t code16_seg_len, uint16_t data_seg, uint16_t data_seg_len);

status_t _pc_bios_disconnect_apm_interface(uint16_t device);

status_t _pc_bios_set_apm_driver_version(uint16_t device, uint16_t version);

status_t _pc_bios_apm_enable_power_management(uint16_t device);

status_t _pc_bios_apm_set_power_state(uint16_t device, uint8_t state);

__noreturn
void _pc_bios_bootnext(void);

#endif // __VELLUM_ASM_BIOS_MISC_H__
