#include <string.h>

#include <stdint.h>

#undef strchr

char *strchr(const char *str, int ch)
{
    while (*str) {
        if (*str == ch) {
            return (char *)str;
        }
        str++;
    }
    return NULL;
}