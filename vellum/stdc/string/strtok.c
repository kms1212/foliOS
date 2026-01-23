#include <string.h>

#include <stdint.h>

char *strtok(char *__restrict str, const char *__restrict delim)
{
    static char *last;

    if (!str && !(str = last)) return NULL;

    // trim leading delimiters
trim_next:
    for (int i = 0; delim[i]; i++) {
        if (*str == delim[i]) {
            str++;
            goto trim_next;
        }
    }

    char *token = str;

    // tokenize
    while (*str) {
        for (int i = 0; delim[i]; i++) {
            if (*str == delim[i]) {
                *str = 0;
                last = ++str;
                return token;
            }
        }
        str++;
    }
    
    last = NULL;

    return token;
}