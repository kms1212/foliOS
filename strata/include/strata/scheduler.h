#ifndef __STRATA_SCHEDULER_H__
#define __STRATA_SCHEDULER_H__

#include <strata/thread.h>

struct StScheduler_Data {
    StThread_InternalRef volatile runqueue_head;
    StThread_InternalRef volatile runqueue_tail;

    StThread_InternalRef volatile current_thread;

    uint64_t context_switch_count;
    uint64_t idle_runtime_ns;
    uint64_t last_maintain_switch_count;
    uint32_t maintain_interval_switches;
    int maintain_requested;
};

StStatus StScheduler_AddThread(StThread_InternalRef th);
void StScheduler_RemoveThread(StThread_InternalRef th);

StStatus StScheduler_GetCurrentThread(StThread_InternalRef *current __out_optional);
StStatus StScheduler_GetNextThread(StThread_InternalRef *next __out_optional);
StStatus StScheduler_SwitchCurrentThread(StThread_InternalRef th __in);
void StScheduler_GetIdleTimeNanoseconds(uint64_t *idle_runtime_ns __out);
void StScheduler_AccountIdleTimeNanoseconds(uint64_t idle_delta_ns __in);

int StScheduler_CheckHasOtherRunnableThread(void);
int StScheduler_ShouldMaintain(void);
void StScheduler_RequestMaintain(void);

StStatus StScheduler_Maintain(void); /* can be refactored to a better name */

#endif  // __STRATA_SCHEDULER_H__
