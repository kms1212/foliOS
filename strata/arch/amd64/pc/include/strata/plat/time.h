#ifndef __STRATA_PLAT_TIME_H__
#define __STRATA_PLAT_TIME_H__

#include <stdint.h>

#include <strata/compiler.h>
#include <strata/status.h>

void StTimeP_EarlyBusyWaitNanoseconds(uint64_t ns __in);
void StTimeP_InitTimer(int use_hpet __in);
StStatus StTimeP_StartTimer(void);
void StTimeP_GetUptimeNanoseconds(uint64_t *uptime_ns __out);

void StTimeP_GetGlobalTick(uint64_t *tick __out);
void StTimeP_GetGlobalTickFrequency(uint32_t *frequency __out);

#endif  // __STRATA_PLAT_TIME_H__
