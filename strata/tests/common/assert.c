#include <stdio.h>
#include <stdlib.h>

#include <strata/mm.h>

void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *func)
{
    fprintf(stderr, "%s:%u: %s: Assertion `%s' failed.\n", file, line, func, assertion);

    StPmm_DebugDumpRegion(0, 1024);
    StPmm_DebugDumpAtpa();

    abort();
}
