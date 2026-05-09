#ifndef __STRATA_PLAT_APIC_H__
#define __STRATA_PLAT_APIC_H__

#include <strata/status.h>

StStatus StApicA_EnableGlobal(void);
StStatus StApicA_EnableLocal(void);

void StApicA_SendEoi(void);

StStatus StApicA_InitLapicTimer(void);
StStatus StApicA_SetLapicTimerPeriodic(uint64_t freq_hz __in);
StStatus StApicA_SetLapicTimerOneshot(uint64_t ns __in);
void StApicP_StopLapicTimer(void);

#endif  // __STRATA_PLAT_APIC_H__
