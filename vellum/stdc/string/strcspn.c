#include <string.h>

#undef strcspn

size_t strcspn(const char *p1, const char *p2)
{
    size_t ret = 0;

    while (*p1) {
        if (strchr(p2, *p1++)) {
            return ret;
        }
        ret++;
    }

    return ret;
}
