#ifndef __STRATA_PLAT_THREAD_H__
#define __STRATA_PLAT_THREAD_H__

#include <strata/plat/interrupt.h>

#include <strata/status.h>
#include <strata/compiler.h>

struct StThreadP_PlatformData {
    int dummy;
};

struct StThread;

StStatus StThreadP_AllocateKThreadStack(struct StThread *th __in);
StStatus StThreadP_SetupKThreadStack(struct StThread *th __in);
void StThreadP_FreeKThreadStack(struct StThread *th __in);

StStatus StThreadP_Switch(
    struct StThread *next __in,
    struct StIntP_Context *ctx __in,
    void **next_stack_ptr __out
);

void StThreadP_Yield(void);

#endif // __STRATA_PLAT_THREAD_H__
