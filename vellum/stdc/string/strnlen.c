#include <string.h>

#include <stdint.h>

#undef strnlen

size_t strnlen(const char *str, size_t maxlen)
{
    size_t s = 0;
    for (; *str++ && s <= maxlen; s++);
    return s;
}

#undef strlen

size_t strlen(const char *str)
{
    size_t s = 0;
    for (; *str++; s++);
    return s;
}