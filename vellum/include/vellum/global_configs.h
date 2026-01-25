#ifndef __VELLUM_GLOBAL_CONFIGS_H__
#define __VELLUM_GLOBAL_CONFIGS_H__

#include <stdint.h>
#include <time.h>

#include <vellum/json.h>

/* from boot:/config/boot.json file */
extern struct json_value *config_data;

extern const char *config_password;
extern time_t config_timezone_offset;
extern int config_rtc_utc;

/* from diagnostic data */
extern int config_rtc_century_offset;
extern uint64_t config_tsc_diff_per_sec;

#endif  // __VELLUM_GLOBAL_CONFIGS_H__
