#include <strata/panic.h>

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <strata/mm.h>

void St_Panic(StStatus status, const char *fmt, ...)
{
    fprintf(stderr, "panic: %08" PRIx32 ": ", status);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    StPmm_DebugDumpRegion(0, 1024);
    StPmm_DebugDumpAtpa();

    abort();
}
