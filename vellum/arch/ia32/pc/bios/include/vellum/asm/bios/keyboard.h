#ifndef __VELLUM_ASM_BIOS_KEYPLATFORM_H__
#define __VELLUM_ASM_BIOS_KEYPLATFORM_H__

#include <stdint.h>

void _pc_bios_keyboard_get_stroke(uint8_t *scancode, char *ascii);
int _pc_bios_keyboard_check_stroke(uint8_t *scancode, char *ascii);
uint16_t _pc_bios_keyboard_get_state(void);

#endif  // __VELLUM_ASM_BIOS_KEYPLATFORM_H__
