#include <time.h>

#include <vellum/device.h>
#include <vellum/global_configs.h>
#include <vellum/interface/rtc.h>
#include <vellum/status.h>

time_t time(time_t *time)
{
    status_t status;
    struct device *rtcdev;
    const struct rtc_interface *rtcif;
    struct rtc_time rtctime;

    status = VlDev_Find("rtc0", &rtcdev);
    if (!CHECK_SUCCESS(status)) return 0;

    status = rtcdev->driver->get_interface(rtcdev, "rtc", (const void **)&rtcif);
    if (!CHECK_SUCCESS(status)) return 0;

    status = rtcif->get_time(rtcdev, &rtctime);
    if (!CHECK_SUCCESS(status)) return 0;

    struct tm newtime = {
        .tm_year = rtctime.year - 1900,
        .tm_mon = rtctime.month - 1,
        .tm_mday = rtctime.mday,
        .tm_hour = rtctime.hour,
        .tm_min = rtctime.minute,
        .tm_sec = rtctime.second,
        .tm_isdst = 0,
    };

    time_t newtimet = mktime(&newtime);

    if (!config_rtc_utc) {
        newtimet -= config_timezone_offset;
    }

    if (time) *time = newtimet;

    return newtimet;
}

size_t strftime(
    char *__restrict str, size_t maxsize, const char *__restrict fmt, const struct tm *__restrict tm
)
{
    return 0;
}

char *asctime(const struct tm *tm)
{
    return NULL;
}

char *ctime(const time_t *timer)
{
    return NULL;
}
