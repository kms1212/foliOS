#include <string.h>

#include <stdint.h>

#undef strncat

char *strncat(char *__restrict dest, const char *__restrict src, size_t maxlen)
{
    char *orig_dest = dest;
    
    while (*dest && maxlen > 0) {
        dest++;
        maxlen--;
    }

    while (*src && maxlen > 0) {
        *dest++ = *src++;
        maxlen--;
    }

    return orig_dest;
}

#undef strcat

char *strcat(char *__restrict dest, const char *__restrict src)
{
    return strncat(dest, src, SIZE_MAX);
}
