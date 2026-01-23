#include <string.h>

#include <vellum/panic.h>

#undef memset

void *__memset_chk(void *dest, int c, size_t len, size_t destlen)
{
    if (destlen < len) {
        panic(STATUS_SIZE_CHECK_FAILURE, "__memset_chk() failed");
    }

#ifndef __HAVE_BUILTIN_MEMSET
    return memset(dest, c, len);

#else
    return __builtin_memset(dest, c, len);

#endif
}
