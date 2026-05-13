#include <stdio.h>

#include "internal.h"  // NOLINT(misc-include-cleaner)

static FILE _stdout = {
    .type = 0,
};

static FILE _stdin = {
    .type = 0,
};

static FILE _stderr = {
    .type = 0,
};

static FILE _stddbg = {
    .type = 0,
};

FILE *stdin = &_stdin, *stdout = &_stdout, *stderr = &_stderr, *stddbg = &_stddbg;
