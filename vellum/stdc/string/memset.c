#include <string.h>

#undef memset

void *memset(void *dest, int c, size_t count)
{
    for (char *dest_c = dest; count > 0; count--) *dest_c++ = c;
    return dest;
}
