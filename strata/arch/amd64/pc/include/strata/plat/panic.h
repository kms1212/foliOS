#ifndef __STRATA_PLAT_PANIC_H__
#define __STRATA_PLAT_PANIC_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/status.h>

__noreturn __format_printf(2, 3) void StP_Panic(StStatus status __in, const char *fmt __in, ...);
__noreturn __format_printf(4, 5) void StP_PanicFromContext(
    StStatus status __in, uintptr_t rbp __in, uintptr_t rip __in, const char *fmt __in, ...
);

#endif  // __STRATA_PLAT_PANIC_H__
