#include <string.h>

#include <vellum/panic.h>

#undef mempcpy

void *__mempcpy_chk(void *dest, const void *src, size_t len, size_t destlen)
{
    if (destlen < len) {
        panic(STATUS_SIZE_CHECK_FAILURE, "__mempcpy_chk() failed");
    }

#ifndef __HAVE_BUILTIN_MEMPCPY
    return mempcpy(dest, src, len);

#else
    return __builtin_mempcpy(dest, src, len);

#endif
}
