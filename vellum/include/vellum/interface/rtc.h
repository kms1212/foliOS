#ifndef __VELLUM_INTERFACE_RTC_H__
#define __VELLUM_INTERFACE_RTC_H__

#include <stdint.h>

#include <vellum/device.h>
#include <vellum/status.h>

struct rtc_time {
    int year, month, mday, hour, minute, second, millisecond;
};

struct rtc_interface {
    VlStatus (*get_time)(struct device *, struct rtc_time *);
    VlStatus (*set_time)(struct device *, const struct rtc_time *);
    VlStatus (*get_alarm)(struct device *, struct rtc_time *);
    VlStatus (*set_alarm)(struct device *, const struct rtc_time *);
};

#endif  // __VELLUM_INTERFACE_RTC_H__
