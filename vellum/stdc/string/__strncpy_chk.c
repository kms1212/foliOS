#include <string.h>

#include <vellum/panic.h>

#undef strncpy

char *__strncpy_chk(char *__restrict dest, const char *__restrict src, size_t len, size_t destlen)
{
    if (destlen < len) {
        panic(STATUS_SIZE_CHECK_FAILURE, "__strncpy_chk() failed");
    }

#ifndef __HAVE_BUILTIN_STRNCPY
    return strncpy(dest, src, len);

#else
    return __builtin_strncpy(dest, src, len);

#endif
}
