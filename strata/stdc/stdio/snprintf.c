#include <stdio.h>

#include <limits.h>
#include <stdarg.h>

int snprintf(char *__restrict buf, size_t size, const char *__restrict fmt, ...)
{
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vsnprintf(buf, size, fmt, args);
    va_end(args);

    return ret;
}

int sprintf(char *__restrict buf, const char *__restrict fmt, ...)
{
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vsnprintf(buf, INT_MAX, fmt, args);
    va_end(args);

    return ret;
}
