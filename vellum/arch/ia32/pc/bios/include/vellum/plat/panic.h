#ifndef __VELLUM_ASM_PANIC_H__
#define __VELLUM_ASM_PANIC_H__

#include <vellum/compiler.h>
#include <vellum/status.h>

__noreturn void VlP_Panic(status_t status, const char *fmt, ...);

#endif  // __VELLUM_ASM_PANIC_H__
