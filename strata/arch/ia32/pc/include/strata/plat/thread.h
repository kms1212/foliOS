#ifndef __STRATA_PLAT_THREAD_H__
#define __STRATA_PLAT_THREAD_H__

#include <strata/status.h>

struct StThreadP_PlatformData {
    uint32_t cr3;
};

struct StThread;

StStatus StThreadP_AllocateKThreadStack(struct StThread *th);
StStatus StThreadP_SetupKThreadStack(struct StThread *th);
void StThreadP_FreeKThreadStack(struct StThread *th);

#endif  // __STRATA_PLAT_THREAD_H__
