#ifndef __VELLUM_ASM_BIOS_APM_H__
#define __VELLUM_ASM_BIOS_APM_H__

#include <stdint.h>

#include <vellum/status.h>

#define APM_PM16_SUPPORTED      0x0001
#define APM_PM32_SUPPORTED      0x0002
#define APM_IDLE_SLOW_CPU       0x0004
#define APM_BIOS_PM_DISABLED    0x0008
#define APM_BIOS_PM_DISENGAGED  0x0010

status_t _pc_bios_apm_check_installation(uint16_t device_id, uint8_t *major_ver, uint8_t *minor_ver, uint16_t *flags);

status_t _pc_bios_apm_connect_rm_interface(void);
status_t _pc_bios_apm_connect_pm16_interface(void **entry, uint16_t *codeseg_base, uint16_t *dataseg_base, uint16_t *codeseg_len, uint16_t *dataseg_len);
status_t _pc_bios_apm_connect_pm32_interface(void **entry, uint16_t *codeseg32_base, uint16_t *codeseg16_base, uint16_t *dataseg_base, uint16_t *codeseg32_len, uint16_t *codeseg16_len, uint16_t *dataseg_len);
status_t _pc_bios_apm_disconnect_interface(void);

#endif // __VELLUM_ASM_BIOS_APM_H__
