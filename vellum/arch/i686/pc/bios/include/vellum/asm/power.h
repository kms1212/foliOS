#ifndef __VELLUM_ASM_POWEROFF_H__
#define __VELLUM_ASM_POWEROFF_H__

#include <vellum/compiler.h>

__noreturn
void _pc_poweroff();

__noreturn
void _pc_reboot();

#define poweroff _pc_poweroff
#define reboot _pc_reboot

#endif // __VELLUM_ASM_POWEROFF_H__
