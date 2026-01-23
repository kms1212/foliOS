#include <time.h>

#include <vellum/global_configs.h>

void localtime_r(const time_t *time, struct tm *_time)
{
    time_t local = *time + config_timezone_offset;

    gmtime_r(&local, _time);
}

struct tm *localtime(const time_t *time)
{
    static struct tm _time;

    localtime_r(time, &_time);

    return &_time;
}
