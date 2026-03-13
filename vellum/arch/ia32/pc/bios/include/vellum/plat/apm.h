#ifndef __VELLUM_ASM_APM_H__
#define __VELLUM_ASM_APM_H__

#include <vellum/status.h>

status_t _pc_apm_init(void);

status_t _pc_apm_cpu_idle(void);
status_t _pc_apm_cpu_busy(void);
status_t _pc_apm_set_power_state(uint16_t device_id, uint16_t state_id);
status_t _pc_apm_enable_global_power_management(uint16_t device_id);
status_t _pc_apm_disable_global_power_management(uint16_t device_id);
status_t _pc_apm_restore_default(uint16_t device_id);
status_t _pc_apm_get_power_status(
    uint16_t device_id,
    uint8_t *ac_status,
    uint8_t *bat_status,
    uint8_t *bat_flags,
    uint8_t *bat_percentage,
    uint16_t *bat_life,
    uint16_t *bat_count
);
status_t _pc_apm_get_power_management_event(uint16_t *code, uint16_t *info);
status_t _pc_apm_get_power_state(uint16_t device_id, uint16_t *state_id);
status_t _pc_apm_enable_power_management(uint16_t device_id);
status_t _pc_apm_disable_power_management(uint16_t device_id);
status_t _pc_apm_set_driver_version(uint8_t major_ver, uint8_t minor_ver);
status_t _pc_apm_engage_power_management_engaged(uint16_t device_id);
status_t _pc_apm_disengage_power_management_engaged(uint16_t device_id);
status_t _pc_apm_get_capabilities(uint16_t device_id, uint8_t *bat_count, uint16_t *cap_flags);
status_t _pc_apm_disable_resume_timer(uint16_t device_id);
status_t _pc_apm_get_resume_timer(
    uint16_t device_id, uint8_t *sec, uint8_t *min, uint8_t *hour, uint16_t *mon_day, uint16_t *year
);
status_t _pc_apm_set_resume_timer(
    uint16_t device_id, uint8_t sec, uint8_t min, uint8_t hour, uint16_t mon_day, uint16_t year
);
status_t _pc_apm_disable_resume_on_ring(uint16_t device_id);
status_t _pc_apm_enable_resume_on_ring(uint16_t device_id);
status_t _pc_apm_get_resume_on_ring_status(uint16_t device_id, int *status);
status_t _pc_apm_disable_timer_based_requests(uint16_t device_id);
status_t _pc_apm_enable_timer_based_requests(uint16_t device_id);
status_t _pc_apm_get_timer_based_requests_status(uint16_t device_id, int *status);

#endif  // __VELLUM_ASM_APM_H__
