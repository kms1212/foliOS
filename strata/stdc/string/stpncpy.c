#include <string.h>

#include <stdint.h>

char *stpncpy(char *__restrict dest, const char *__restrict src, size_t maxlen)
{
    char *d = dest;
    const char *s = src;
    while (maxlen-- && *s)
        *d++ = *s++;
    if (maxlen) *d = 0;
    return d;
}

char *stpcpy(char *__restrict dest, const char *__restrict src)
{
    char *d = dest;
    const char *s = src;
    while (*s)
        *d++ = *s++;
    return d;
}
