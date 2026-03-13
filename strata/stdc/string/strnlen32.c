#include <string.h>

#include <stdint.h>

size_t strnlen32(const void *str, size_t maxlen)
{
    size_t s = 0;
    const uint32_t *str32 = str;
    for (; *str32++ && s <= maxlen; s++)
        ;
    return s;
}

size_t strlen32(const void *str)
{
    size_t s = 0;
    const uint32_t *str32 = str;
    for (; *str32++; s++)
        ;
    return s;
}
