#include <string.h>

#include <vellum/panic.h>

#undef memcpy

void *__memcpy_chk(void *__restrict dest, const void *__restrict src, size_t len, size_t destlen)
{
    if (destlen < len) {
        panic(STATUS_SIZE_CHECK_FAILURE, "__memcpy_chk() failed");
    }

#ifndef __HAVE_BUILTIN_MEMCPY
    return memcpy(dest, src, len);

#else
    return __builtin_memcpy(dest, src, len);

#endif
}
