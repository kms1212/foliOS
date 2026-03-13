#include <vellum/log.h>

#include <stdio.h>

#include <vellum/device.h>
#include <vellum/interface/rtc.h>
#include <vellum/status.h>

#ifdef NDEBUG
static int log_level = LL_NONE;

#else
static int log_level = LL_DEBUG;

#endif

void VlLog_SetLevel(int level)
{
    if (level < -1) level = -1;
    if (level > 4) level = 4;

    log_level = level;
}

void VlLog_Print(int level, const char *module_name, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    VlLog_PrintValist(level, module_name, fmt, args);
    va_end(args);
}

void VlLog_IntSafePrint(int level, const char *module_name, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    VlLog_IntSafePrintValist(level, module_name, fmt, args);
    va_end(args);
}

static const char *ll_str[] = {
    "FATAL",
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG",
    "TRACE",
};

void VlLog_PrintValist(int level, const char *module_name, const char *fmt, va_list args)
{
    if (log_level < level) return;

    status_t status;
    int time_available = 1;
    struct device *rtcdev;
    const struct rtc_interface *rtcif;
    struct rtc_time rtctime;

    status = VlDev_Find("rtc0", &rtcdev);
    if (!CHECK_SUCCESS(status)) {
        time_available = 0;
        goto skip_time;
    }

    status = rtcdev->driver->get_interface(rtcdev, "rtc", (const void **)&rtcif);
    if (!CHECK_SUCCESS(status)) {
        time_available = 0;
        goto skip_time;
    }

    status = rtcif->get_time(rtcdev, &rtctime);
    if (!CHECK_SUCCESS(status)) {
        time_available = 0;
        goto skip_time;
    }

skip_time:
    if (time_available) {
        fprintf(
            stdout,
            "%d-%02d-%02dT%02d:%02d:%02d.%03dZ %s [%s] ",
            rtctime.year,
            rtctime.month,
            rtctime.mday,
            rtctime.hour,
            rtctime.minute,
            rtctime.second,
            rtctime.millisecond,
            module_name,
            ll_str[level]
        );
    } else {
        fprintf(stdout, "%s [%s] ", module_name, ll_str[level]);
    }
    vfprintf(stdout, fmt, args);
}

void VlLog_IntSafePrintValist(int level, const char *module_name, const char *fmt, va_list args)
{
    if (log_level < level) return;

    status_t status;
    int time_available = 1;
    struct device *rtcdev;
    const struct rtc_interface *rtcif;
    struct rtc_time rtctime;

    status = VlDev_Find("rtc0", &rtcdev);
    if (!CHECK_SUCCESS(status)) {
        time_available = 0;
        goto skip_time;
    }

    status = rtcdev->driver->get_interface(rtcdev, "rtc", (const void **)&rtcif);
    if (!CHECK_SUCCESS(status)) {
        time_available = 0;
        goto skip_time;
    }

    status = rtcif->get_time(rtcdev, &rtctime);
    if (!CHECK_SUCCESS(status)) {
        time_available = 0;
        goto skip_time;
    }

skip_time:
    if (time_available) {
        fprintf(
            stddbg,
            "%d-%02d-%02dT%02d:%02d:%02d.%03dZ %s [%s] ",
            rtctime.year,
            rtctime.month,
            rtctime.mday,
            rtctime.hour,
            rtctime.minute,
            rtctime.second,
            rtctime.millisecond,
            module_name,
            ll_str[level]
        );
    } else {
        fprintf(stddbg, "%s [%s] ", module_name, ll_str[level]);
    }
    vfprintf(stddbg, fmt, args);
}
