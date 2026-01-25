#ifndef __STRATA_PLAT_TIME_H__
#define __STRATA_PLAT_TIME_H__

#include <stdint.h>

uint64_t StTimeP_GetGlobalTick(void);
uint32_t StTimeP_GetGlobalTickFrequency(void);

#endif  // __STRATA_PLAT_TIME_H__
