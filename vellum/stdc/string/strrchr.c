#include <string.h>

#include <stdint.h>

#undef strrchr

char *strrchr(const char *str, int ch)
{
    char *found = NULL;
    while (*str) {
        if (*str == ch) {
            found = (char *)str;
        }
        str++;
    }
    return found;
}