#include <stdio.h>

#include <limits.h>

#undef vsprintf

int vsprintf(char *__restrict buf, const char *__restrict fmt, va_list args)
{
    return vsnprintf(buf, INT_MAX, fmt, args);
}