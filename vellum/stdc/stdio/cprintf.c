#include <stdio.h>

int cprintf(int (*func)(void *, char), void *farg, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vcprintf(func, farg, fmt, args);
    va_end(args);
    return ret;
}
