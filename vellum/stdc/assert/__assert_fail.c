#include <assert.h>

#include <stdio.h>

void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *func)
{
    fprintf(stderr, "assertion failed: %s: %d: (%s): %s\n", file, line, func, assertion);

    for (;;) {}
}
