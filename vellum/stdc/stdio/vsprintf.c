#include <stdio.h>

#include <limits.h>

int vsprintf(char *__restrict buf, const char *__restrict fmt, va_list args)
{
    return vsnprintf(buf, INT_MAX, fmt, args);
}
