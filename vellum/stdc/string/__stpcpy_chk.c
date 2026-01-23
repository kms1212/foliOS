#include <string.h>

#include <vellum/panic.h>

#undef stpcpy

char *__stpcpy_chk(char *dest, const char *src, size_t destlen)
{
    size_t len = strlen(src) + 1;

    if (destlen < len) {
        panic(STATUS_SIZE_CHECK_FAILURE, "__stpcpy_chk() failed");
    }

#ifndef __HAVE_BUILTIN_STPCPY
    return stpcpy(dest, src);

#else
    return __builtin_stpcpy(dest, src);

#endif
}
