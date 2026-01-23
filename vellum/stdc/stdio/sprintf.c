#include <stdio.h>

#include <limits.h>

#undef sprintf

int sprintf(char *__restrict buf, const char *__restrict fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, INT_MAX, fmt, args);
    va_end(args);
    return ret;
}