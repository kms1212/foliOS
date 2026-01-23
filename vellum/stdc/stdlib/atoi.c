#include <stdlib.h>

#include <errno.h>
#include <limits.h>

int atoi(const char *str)
{
    int ret = 0;
    int sign = 0; // 0: positive 1: negative

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

    while (*str) {
        int digit = 0;
        if ('0' <= *str && *str <= '9') {
            digit = *str - '0';
        } else {
            errno = EINVAL;
            break;
        }

        ret *= 10;
        ret += digit;

        // check range
        if (ret < 0) {  // we didn't applied sign yet, so it can't be negative
            errno = ERANGE;
            if (sign) {
                return INT_MIN;
            } else {
                return INT_MAX;
            }
        }
        str++;
    }

    if (sign) {
        ret = -ret;
    }

    return ret;
}
