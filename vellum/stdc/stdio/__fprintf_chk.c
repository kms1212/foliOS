#include <stdio.h>

#include <vellum/panic.h>

#undef fprintf

int __fprintf_chk(FILE *__restrict stream, int flag, const char *__restrict fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = vfprintf(stream, fmt, args);
    va_end(args);

    return len;
}
