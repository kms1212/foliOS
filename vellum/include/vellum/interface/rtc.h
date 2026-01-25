#ifndef __EMOS_INTERFACE_RTC_H__
#define __EMOS_INTERFACE_RTC_H__

#include <stdint.h>

#include <vellum/device.h>
#include <vellum/status.h>

struct rtc_time {
    int year, month, mday, hour, minute, second, millisecond;
};

struct rtc_interface {
    status_t (*get_time)(struct device *, struct rtc_time *);
    status_t (*set_time)(struct device *, const struct rtc_time *);
    status_t (*get_alarm)(struct device *, struct rtc_time *);
    status_t (*set_alarm)(struct device *, const struct rtc_time *);
};

#endif  // __EMOS_INTERFACE_RTC_H__
