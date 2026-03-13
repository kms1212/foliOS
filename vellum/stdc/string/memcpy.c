#include <string.h>

#include <vellum/panic.h>

void *memcpy(void *__restrict dest, const void *__restrict src, size_t len)
{
    char *dest_c = dest;
    const char *src_c = src;
    while (len--)
        *dest_c++ = *src_c++;
    return dest;
}
