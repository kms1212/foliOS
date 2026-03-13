#ifndef __VELLUM_ASM_BIOS_KEYPLATFORM_H__
#define __VELLUM_ASM_BIOS_KEYPLATFORM_H__

#include <stdint.h>

void VlBiosP_GetKeyboardStroke(uint8_t *scancode, char *ascii);
int VlBiosP_CheckKeyboardSroke(uint8_t *scancode, char *ascii);
uint16_t VlBiosP_GetKeyboardState(void);

#endif  // __VELLUM_ASM_BIOS_KEYPLATFORM_H__
