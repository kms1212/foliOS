#ifndef __STRATA_PLAT_TIME_H__
#define __STRATA_PLAT_TIME_H__

#include <stdint.h>

#include <strata/compiler.h>

void StTimeP_EarlyBusyWaitMicroseconds(uint32_t us);
void StTimeP_InitTimer(int use_hpet __in);
uint64_t StTimeP_GetUptimeNanoseconds(void);

uint64_t StTimeP_GetGlobalTick(void);
uint32_t StTimeP_GetGlobalTickFrequency(void);

#endif  // __STRATA_PLAT_TIME_H__
