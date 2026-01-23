#include <string.h>

#include <vellum/panic.h>

#undef strcat

void *__strcat_chk(void *__restrict dest, const void *__restrict src, size_t destlen)
{
    size_t len = strlen(src) + 1;

    if (destlen < len) {
        panic(STATUS_SIZE_CHECK_FAILURE, "__strcat_chk() failed");
    }

#ifndef __HAVE_BUILTIN_STRCAT
    return strcat(dest, src);

#else
    return __builtin_strcat(dest, src);

#endif
}
