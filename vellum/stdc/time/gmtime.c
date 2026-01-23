#include <time.h>

void gmtime_r(const time_t *_time, struct tm *result)
{
    time_t time = *_time;
    
    result->tm_sec = time % 60;
    time /= 60;
    result->tm_min = time % 60;
    time /= 60;
    result->tm_hour = time % 24;
    time_t days = time / 24;

    days += 719468;

    time_t era = (days >= 0 ? days : days - 146096) / 146097;
    unsigned doe = (unsigned)(days - era * 146097);
    
    unsigned yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    time_t y = (time_t)yoe + era * 400;
    
    unsigned doy = doe - (365 * yoe + yoe/4 - yoe/100);
    
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1;
    unsigned m = mp < 10 ? mp + 3 : mp - 9;

    result->tm_year = (int)(y + (m <= 2)) - 1900;
    result->tm_mon = m - 1;
    result->tm_mday = d;
    
    int is_leap = (!( (result->tm_year + 1900) % 4) && ( (result->tm_year + 1900) % 100)) || !( (result->tm_year + 1900) % 400);
    
    if (m <= 2) {
        if (m == 1) result->tm_yday = d - 1;
        else result->tm_yday = 31 + d - 1;
    } else {
        result->tm_yday = doy + 58 + is_leap;
    }
}

struct tm *gmtime(const time_t *time)
{
    static struct tm _time;

    gmtime_r(time, &_time);

    return &_time;
}
