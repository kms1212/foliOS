#ifndef __VELLUM_ASM_BIOS_MISC_H__
#define __VELLUM_ASM_BIOS_MISC_H__

#include <stdint.h>

#include <vellum/compiler.h>
#include <vellum/status.h>

VlStatus VlBiosP_CheckApmVersion(uint16_t *version);

VlStatus VlBiosP_ConnectApmInterface(
    uint16_t device,
    void (*entry_point)(void),
    uint16_t code32_seg,
    uint16_t code32_seg_len,
    uint16_t code16_seg,
    uint16_t code16_seg_len,
    uint16_t data_seg,
    uint16_t data_seg_len
);

VlStatus VlBiosP_DisconnectApmInterface(uint16_t device);

VlStatus VlBiosP_SetApmDriverVersion(uint16_t device, uint16_t version);

VlStatus VlBiosP_EnableApmPowerManagement(uint16_t device);

VlStatus VlBiosP_SetApmPowerState(uint16_t device, uint8_t state);

__noreturn void VlBiosP_BootFromNextDevice(void);

#endif  // __VELLUM_ASM_BIOS_MISC_H__
