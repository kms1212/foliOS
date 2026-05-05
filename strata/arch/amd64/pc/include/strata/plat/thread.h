#ifndef __STRATA_PLAT_THREAD_H__
#define __STRATA_PLAT_THREAD_H__

#include <strata/arch/intrinsics/fpu_simd.h>
#include <strata/plat/interrupt.h>

#include <strata/compiler.h>
#include <strata/mm/types.h>
#include <strata/status.h>

struct StThreadP_PlatformData {
    uintptr_t fs_base;
    uintptr_t gs_base;
    union StA_XStateBuffer *xstate_buffer;
};

struct StThread;

StStatus StThreadP_InitializeFpuSimdState(void);
void StThreadP_InitializePlatformData(struct StThread *th __inout);
void StThreadP_FreePlatformData(struct StThread *th __inout);

StStatus StThreadP_AllocateThreadKernelStack(struct StThread *th __in);
StStatus StThreadP_SetupThreadKernelStack(struct StThread *th __in);
void StThreadP_FreeThreadKernelStack(struct StThread *th __in);
St_PageCount StThreadP_ReclaimCachedKernelStacks(St_PageCount page_budget __in);

StStatus StThreadP_AllocateThreadUserStack(struct StThread *th __in);
StStatus StThreadP_SetupThreadUserStack(
    struct StThread *th __in,
    int arg_count,
    const char *const *args,
    int env_count,
    const char *const *envs
);
void StThreadP_FreeThreadUserStack(struct StThread *th __in);

StStatus StThreadP_SetFsBase(struct StThread *th __in, uintptr_t fs_base __in);
StStatus StThreadP_SetGsBase(struct StThread *th __in, uintptr_t gs_base __in);

StStatus StThreadP_Switch(
    struct StThread *next __in, struct StIntP_Context *ctx __in, void **next_stack_ptr __out
);

void StThreadP_Yield(void);
void StThreadP_IdleUntilInterrupt(void);

#endif  // __STRATA_PLAT_THREAD_H__
