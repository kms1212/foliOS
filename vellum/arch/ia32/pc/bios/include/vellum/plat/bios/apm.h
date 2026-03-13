#ifndef __VELLUM_ASM_BIOS_APM_H__
#define __VELLUM_ASM_BIOS_APM_H__

#include <stdint.h>

#include <vellum/status.h>

#define APM_PM16_SUPPORTED     0x0001
#define APM_PM32_SUPPORTED     0x0002
#define APM_IDLE_SLOW_CPU      0x0004
#define APM_BIOS_PM_DISABLED   0x0008
#define APM_BIOS_PM_DISENGAGED 0x0010

status_t VlBiosP_CheckApmInstallation(
    uint16_t device_id, uint8_t *major_ver, uint8_t *minor_ver, uint16_t *flags
);

status_t VlBiosP_ConnectApmRmInterface(void);
status_t VlBiosP_ConnectApmPm16Interface(
    void **entry,
    uint16_t *codeseg_base,
    uint16_t *dataseg_base,
    uint16_t *codeseg_len,
    uint16_t *dataseg_len
);
status_t VlBiosP_ConnectPm32Interface(
    void **entry,
    uint16_t *codeseg32_base,
    uint16_t *codeseg16_base,
    uint16_t *dataseg_base,
    uint16_t *codeseg32_len,
    uint16_t *codeseg16_len,
    uint16_t *dataseg_len
);
status_t VlBiosP_DisconnectApmInterface(void);

#endif  // __VELLUM_ASM_BIOS_APM_H__
