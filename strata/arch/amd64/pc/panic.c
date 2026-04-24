#include <strata/plat/panic.h>

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>

#include <strata/arch/intrinsics/io.h>
#include <strata/arch/intrinsics/misc.h>

#include <strata/compiler.h>
#include <strata/status.h>

static int panic_out(void *data, char ch)
{
    if (!ch) return 1;

    StIoA_Out8(0x00E9, ch);

    return 0;
}

__noreturn void StP_Panic(StStatus status, const char *fmt, ...)
{
    va_list args;

    cprintf(panic_out, NULL, "panic: %" PRIX32 ", ", status);

    va_start(args, fmt);
    vcprintf(panic_out, NULL, fmt, args);
    va_end(args);

    StA_Cli();
    for (;;) {
        StA_Hlt();
    }
}
