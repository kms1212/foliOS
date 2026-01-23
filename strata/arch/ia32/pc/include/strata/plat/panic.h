#ifndef __STRATA_PLAT_PANIC_H__
#define __STRATA_PLAT_PANIC_H__

#include <strata/compiler.h>
#include <strata/status.h>

__noreturn
void StP_Panic(StStatus status, const char *fmt, ...);

#endif // __STRATA_PLAT_PANIC_H__
