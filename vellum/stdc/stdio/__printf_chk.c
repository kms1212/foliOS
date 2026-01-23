#include <stdio.h>

#undef printf

int __printf_chk(int flag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = vprintf(fmt, args);
    va_end(args);

    return len;
}
