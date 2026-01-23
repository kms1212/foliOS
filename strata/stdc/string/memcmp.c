#include <string.h>

#include <stdint.h>

#undef memcmp

int memcmp(const void *p1, const void *p2, size_t len)
{
    const uint8_t *a = p1, *b = p2;
    while (len--) {
        if (*a != *b) return (*a - *b);
        a++;
        b++;
    }
    return 0;
}
