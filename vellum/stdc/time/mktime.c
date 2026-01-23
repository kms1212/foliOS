#include <time.h>

time_t mktime(struct tm *tm) {
    long year = tm->tm_year + 1900;
    long mon = tm->tm_mon;
    time_t days;

    if (mon < 2) {
        mon += 12;
        year -= 1;
    }

    days = (year * 365) + (year / 4) - (year / 100) + (year / 400);
    days += (153 * (mon - 2) + 2) / 5 + tm->tm_mday - 1;
    days -= 719468;  // convert to unix epoch day.

    return ((time_t)days * 86400) + ((time_t)tm->tm_hour * 3600) + (time_t)(tm->tm_min * 60) + (time_t)tm->tm_sec;
}
