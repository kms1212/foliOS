#ifndef __STRATA_SCHEDULER_H__
#define __STRATA_SCHEDULER_H__

#include <strata/thread.h>

struct StScheduler_Data {
    struct StThread *volatile runqueue_head;
    struct StThread *volatile runqueue_tail;

    struct StThread *volatile current_thread;

    uint64_t context_switch_count;
};

StStatus StScheduler_AddThread(struct StThread *th);
StStatus StScheduler_RemoveThread(struct StThread *th);

StStatus StScheduler_GetCurrentThread(struct StThread **current);
StStatus StScheduler_GetNextThread(struct StThread **next);
StStatus StScheduler_SetCurrentThread(struct StThread *th);

int StScheduler_CheckHasOtherRunnableThread(void);

StStatus StScheduler_Maintain(void); /* can be refactored to a better name */

#endif  // __STRATA_SCHEDULER_H__
