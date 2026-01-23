#include <string.h>

#undef memmove

void *memmove(void *dest, const void *src, size_t len)
{
    char *dest_c = dest;
    const char *src_c = src;

    if ((long)dest > (long)src) {
        dest_c += len;
        src_c += len;

        while (len--) *--dest_c = *--src_c;
    } else {
        while (len--) *dest_c++ = *src_c++;
    }
    
    return dest;
}
