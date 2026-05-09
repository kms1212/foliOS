#ifndef __STRATA_SCHEDULER_H__
#define __STRATA_SCHEDULER_H__

#include <strata/thread.h>

struct StScheduler_Data {
    struct StThread *volatile runqueue_head;
    struct StThread *volatile runqueue_tail;

    struct StThread *volatile current_thread;

    uint64_t context_switch_count;
    uint64_t idle_runtime_ns;
    uint64_t last_maintain_switch_count;
    uint32_t maintain_interval_switches;
    int maintain_requested;
};

StStatus StScheduler_AddThread(struct StThread *th);
StStatus StScheduler_RemoveThread(struct StThread *th);

StStatus StScheduler_GetCurrentThread(struct StThread **current);
StStatus StScheduler_GetNextThread(struct StThread **next);
StStatus StScheduler_SwitchCurrentThread(struct StThread *th);
StStatus StScheduler_GetIdleTimeNanoseconds(uint64_t *idle_runtime_ns);
void StScheduler_AccountIdleTimeNanoseconds(uint64_t idle_delta_ns);

int StScheduler_CheckHasOtherRunnableThread(void);
int StScheduler_ShouldMaintain(void);
void StScheduler_RequestMaintain(void);

StStatus StScheduler_Maintain(void); /* can be refactored to a better name */

#endif  // __STRATA_SCHEDULER_H__
