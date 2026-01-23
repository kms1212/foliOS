#include <stdlib.h>

#include <errno.h>
#include <limits.h>

unsigned long strtoul(const char *__restrict str, char **__restrict endptr, int base)
{
    unsigned long ret = 0;

    if (base < 0 || base == 1 || base > 36) {
        errno = EINVAL;
        return 0;
    }

    while (*str && *str == ' ') {  // trim leading spaces
        str++;
    }

    if (*str == '0') {
        if (str[1] != 'x' && str[1] != 'X') {
            if (!base) {
                base = 8;
            }
        } else {
            str++;
            if (!base) {
                base = 16;
            }
        }
        str++;
    }

    while (*str) {
        int digit = 0;
        if ('0' <= *str && *str <= '9') {
            digit = *str - '0';
        } else if ('a' <= *str && *str <= 'z') {
            digit = *str - 'a' + 10;
        } else if ('A' <= *str && *str <= 'Z') {
            digit = *str - 'A' + 10;
        } else {
            errno = EINVAL;
            break;
        }

        // check base
        if (digit >= base) {
            errno = EINVAL;
            break;
        }

        ret *= base;
        ret += digit;

        str++;
    }

    if (endptr) {
        *endptr = (char *)str;
    }

    return ret;
}