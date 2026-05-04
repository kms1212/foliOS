#ifndef __STRATA_PLAT_PIT_H__
#define __STRATA_PLAT_PIT_H__

#include <strata/compiler.h>
#include <strata/status.h>

StStatus StPitP_Init(void);
StStatus StPitP_SetPeriodic(uint64_t freq_hz __in);
StStatus StPitP_SetOneshot(uint64_t us __in);
void StPitP_SetOneshotAndBusyWait(uint64_t us __in);
void StPitP_Stop(void);

#endif  // __STRATA_PLAT_PIT_H__
