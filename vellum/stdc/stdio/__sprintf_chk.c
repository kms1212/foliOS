#include <stdio.h>

#include <vellum/panic.h>

int __sprintf_chk(char *str, int flag, size_t slen, const char *fmt, ...)
{
    if (slen < 1) {
        VlP_Panic(STATUS_SIZE_CHECK_FAILURE, "__sprintf_chk() failed");
    }

    va_list args;
    va_start(args, fmt);
    int len = vsprintf(str, fmt, args);
    va_end(args);

    return len;
}
