#ifndef __TIME_H__
#define __TIME_H__

#include <stdint.h>
#include <stddef.h>

#define CLOCKS_PER_SEC 1024

typedef int64_t clock_t;
typedef int64_t time_t;

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

clock_t clock(void);

time_t time(time_t *time);

time_t mktime(struct tm *tm);

struct tm *gmtime(const time_t *time);

struct tm *localtime(const time_t *time);

size_t strftime(char *__restrict str, size_t maxsize, const char *__restrict fmt, const struct tm *__restrict tm);

char *asctime(const struct tm *tm);

char *ctime(const time_t *timer);


#endif // __TIME_H__
