#include <stdlib.h>

#include <errno.h>
#include <limits.h>

long strtol(const char *__restrict str, char **__restrict endptr, int base)
{
    long ret = 0;
    int sign = 0; // 0: positive 1: negative

    if (base < 0 || base == 1 || base > 36) {
        errno = EINVAL;
        return 0;
    }

    while (*str && *str == ' ') {  // trim leading spaces
        str++;
    }

    if (*str == '+') {
        sign = 0;
        str++;
    } else if (*str == '-') {
        sign = 1;
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

        // check range
        if (ret < 0) {  // we didn't applied sign yet, so it can't be negative
            errno = ERANGE;
            if (sign) {
                return LONG_MIN;
            } else {
                return LONG_MAX;
            }
        }
        str++;
    }

    if (sign) {
        ret = -ret;
    }

    if (endptr) {
        *endptr = (char *)str;
    }

    return ret;
}