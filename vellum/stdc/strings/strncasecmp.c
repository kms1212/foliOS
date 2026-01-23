#include <string.h>

#include <stdint.h>
#include <ctype.h>

int strncasecmp(const char *p1, const char *p2, size_t maxlen)
{
    while (--maxlen) {
        if (!*p1 || toupper(*p1) != toupper(*p2)) break;
        p1++;
        p2++;
    }
    return (*(unsigned char *)p1 - *(unsigned char *)p2);
}

int strcasecmp(const char *p1, const char *p2)
{
    return strncasecmp(p1, p2, SIZE_MAX);
}