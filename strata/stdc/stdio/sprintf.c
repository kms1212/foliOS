#include <stdio.h>

#include <limits.h>

#undef sprintf

int sprintf(char *__restrict buf, const char *__restrict fmt, ...)
{
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vsnprintf(buf, INT_MAX, fmt, args);
    va_end(args);

    return ret;
}
