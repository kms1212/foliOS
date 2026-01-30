#include <string.h>

#include <limits.h>
#include <stdlib.h>

#include <vellum/macros.h>

char *strndup(const char *str, size_t size)
{
    size_t slen = strnlen(str, size);
    slen = MIN(slen + 1, size);

    char *result = malloc(slen);
    if (!result) return NULL;

    strncpy(result, str, slen);

    return result;
}

char *strdup(const char *str)
{
    return strndup(str, SIZE_MAX / 2 - 1);
}
