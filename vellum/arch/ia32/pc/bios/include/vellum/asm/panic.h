#ifndef __VELLUM_ASM_PANIC_H__
#define __VELLUM_ASM_PANIC_H__

#include <vellum/compiler.h>
#include <vellum/status.h>

__noreturn void _pc_panic(status_t status, const char *fmt, ...);

#define panic _pc_panic

#endif  // __VELLUM_ASM_PANIC_H__
