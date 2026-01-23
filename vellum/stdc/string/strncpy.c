#include <string.h>

#include <stdint.h>

#undef strncpy

char *strncpy(char *__restrict dest, const char *__restrict src, size_t maxlen)
{
    char *d = dest;
    const char *s = src;
    while (maxlen-- && *s) *d++ = *s++;
    if (maxlen) *d = 0;
    return dest;
}

#undef strcpy

char *strcpy(char *__restrict dest, const char *__restrict src)
{
    return strncpy(dest, src, SIZE_MAX);
}