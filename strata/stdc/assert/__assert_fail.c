#include <assert.h>

#include <strata/plat/panic.h>

#include <strata/status.h>

void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *func)
{
    StP_Panic(
        STATUS_ASSERTION_FAILED,
        "assertion failed: %s: %d: (%s): %s\n",
        file,
        line,
        func,
        assertion
    );

    for (;;) {
    }
}
