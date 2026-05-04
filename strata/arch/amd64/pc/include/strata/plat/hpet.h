#ifndef __STRATA_PLAT_HPET_H__
#define __STRATA_PLAT_HPET_H__

#include <strata/compiler.h>
#include <strata/status.h>

StStatus StHpetP_Init(void);
int StHpetP_IsInitialized(void);
uint64_t StHpetP_GetMainCounter(void);
uint64_t StHpetP_GetCounterFrequency(void);
StStatus StHpetP_SetPeriodic(uint64_t freq_hz __in);
StStatus StHpetP_SetOneshot(uint64_t us __in);
StStatus StHpetP_SetOneshotAndBusyWait(uint64_t us __in);
void StHpetP_Stop(void);

#endif  // __STRATA_PLAT_HPET_H__
