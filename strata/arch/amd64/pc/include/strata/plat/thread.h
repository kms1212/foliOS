#ifndef __STRATA_PLAT_THREAD_H__
#define __STRATA_PLAT_THREAD_H__

#include <strata/status.h>

#include <strata/plat/interrupt.h>

struct StThreadP_PlatformData {
    uintptr_t user_rsp;
};

struct StThread;

StStatus StThreadP_AllocateKThreadStack(struct StThread *th);
StStatus StThreadP_SetupKThreadStack(struct StThread *th);
void StThreadP_FreeKThreadStack(struct StThread *th);

StStatus StThreadP_Switch(struct StThread *next, struct StIntP_Context *ctx, void **next_stack_ptr);

#endif // __STRATA_PLAT_THREAD_H__
