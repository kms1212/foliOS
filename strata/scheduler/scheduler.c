#include <strata/scheduler.h>

#include <limits.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/time.h>

#include <strata/log.h>
#include <strata/status.h>
#include <strata/thread.h>

#define MODULE_NAME                              "sched"
#define SCHED_DEFAULT_MAINTAIN_INTERVAL_SWITCHES (64U)

static void ensure_scheduler_defaults(struct StScheduler_Data *scheduler)
{
    if (!scheduler->maintain_interval_switches) {
        scheduler->maintain_interval_switches = SCHED_DEFAULT_MAINTAIN_INTERVAL_SWITCHES;
    }
}

static uint64_t get_min_runnable_pass(
    const struct StScheduler_Data *scheduler, const struct StThread *exclude
)
{
    uint64_t min_pass = UINT64_MAX;

    for (const struct StThread *current = scheduler->runqueue_head; current;
         current = current->next) {
        if (current == exclude) continue;
        if (current->state != THREAD_STATE_RUNNING) continue;
        if (current->sched_pass < min_pass) min_pass = current->sched_pass;
    }

    return min_pass;
}

static void catch_up_runnable_pass(struct StScheduler_Data *scheduler, struct StThread *thread)
{
    uint64_t min_pass;

    if (!thread || thread->state != THREAD_STATE_RUNNING) return;

    min_pass = get_min_runnable_pass(scheduler, thread);
    if (min_pass == UINT64_MAX) return;

    if (thread->sched_pass < min_pass) {
        thread->sched_pass = min_pass;
    }
}

static void update_runnable_state(
    struct StScheduler_Data *scheduler, struct StThread *thread, uint64_t now_us
)
{
    switch (thread->state) {
    case THREAD_STATE_PENDING:
        thread->state = THREAD_STATE_RUNNING;
        catch_up_runnable_pass(scheduler, thread);
        break;
    case THREAD_STATE_SLEEPING:
        if (now_us >= thread->sleep_until_uptime_us) {
            thread->state = THREAD_STATE_RUNNING;
            thread->sleep_until_uptime_us = 0;
            catch_up_runnable_pass(scheduler, thread);
        }
        break;
    default:
        break;
    }
}

static void account_thread_runtime(struct StThread *thread, uint64_t now_us)
{
    uint64_t delta_us;

    if (!thread) return;
    if (!thread->last_scheduled_in_us) return;
    if (now_us <= thread->last_scheduled_in_us) return;

    delta_us = now_us - thread->last_scheduled_in_us;
    thread->runtime_total_us += delta_us;
    thread->last_scheduled_in_us = now_us;
}

static struct StThread *select_next_runnable_thread(
    struct StScheduler_Data *scheduler, uint64_t now_us, int update_accounting
)
{
    struct StThread *start;
    struct StThread *cursor;
    struct StThread *best = NULL;
    uint64_t best_pass = UINT64_MAX;

    if (!scheduler->runqueue_head) {
        return NULL;
    }

    start = scheduler->current_thread ? scheduler->current_thread->next : scheduler->runqueue_head;
    if (!start) start = scheduler->runqueue_head;
    cursor = start;

    do {
        update_runnable_state(scheduler, cursor, now_us);

        if (cursor->state == THREAD_STATE_RUNNING) {
            if (!best || cursor->sched_pass < best_pass) {
                best = cursor;
                best_pass = cursor->sched_pass;
            }
        }

        cursor = cursor->next ? cursor->next : scheduler->runqueue_head;
    } while (cursor && cursor != start);

    if (best && update_accounting) {
        best->sched_pass++;
        best->sched_run_count++;
    }

    return best;
}

StStatus StScheduler_AddThread(struct StThread *th)
{
    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;
    uint64_t initial_pass = 0;

    ensure_scheduler_defaults(scheduler);

    if (scheduler->current_thread) {
        initial_pass = scheduler->current_thread->sched_pass;
    }

    th->sched_pass = initial_pass;
    th->sched_run_count = 0;
    th->runtime_total_us = 0;
    th->last_scheduled_in_us = 0;

    if (!scheduler->runqueue_head) {
        scheduler->runqueue_head = th;
        scheduler->runqueue_tail = th;
    } else {
        scheduler->runqueue_tail->next = th;
        scheduler->runqueue_tail = th;
    }
    th->next = NULL;

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "thread #%d added to scheduler\n", th->id);

    return STATUS_SUCCESS;
}

StStatus StScheduler_RemoveThread(struct StThread *th)
{
    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;

    if (th->state != THREAD_STATE_FINISHED) {
        return STATUS_THREAD_NOT_FINISHED;
    }

    if (th == scheduler->runqueue_head) {
        scheduler->runqueue_head = th->next;
        if (!scheduler->runqueue_head) {
            scheduler->runqueue_tail = NULL;
        }
    }

    for (struct StThread *current = scheduler->runqueue_head; current && current->next;
         current = current->next) {
        if (th == current->next) {
            current->next = th->next;
            if (th == scheduler->runqueue_tail) {
                scheduler->runqueue_tail = current;
            }
            break;
        }
    }

    LOG_TRACE(LM_CAT_UNCLASSIFIED, "thread #%d removed from scheduler\n", th->id);

    return STATUS_SUCCESS;
}

StStatus StScheduler_GetCurrentThread(struct StThread **current)
{
    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;

    if (current) *current = scheduler->current_thread;

    return STATUS_SUCCESS;
}

StStatus StScheduler_GetNextThread(struct StThread **next)
{
    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;
    uint64_t now_us = StTimeP_GetUptimeNanoseconds() / 1000;

    if (next) {
        *next = select_next_runnable_thread(scheduler, now_us, 1);
    } else {
        (void)select_next_runnable_thread(scheduler, now_us, 1);
    }

    return STATUS_SUCCESS;
}

StStatus StScheduler_SwitchCurrentThread(struct StThread *th)
{
    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;
    struct StThread *prev;
    uint64_t now_us;

    if (!th) {
        return STATUS_INVALID_VALUE;
    }

    ensure_scheduler_defaults(scheduler);

    prev = scheduler->current_thread;
    now_us = StTimeP_GetUptimeNanoseconds() / 1000;

    if (prev && prev != th) {
        account_thread_runtime(prev, now_us);
    }

    scheduler->current_thread = th;
    if (prev != th || !th->last_scheduled_in_us) {
        th->last_scheduled_in_us = now_us;
    }

    if (prev && prev != th) {
        scheduler->context_switch_count++;
    }

    return STATUS_SUCCESS;
}

StStatus StScheduler_GetIdleRuntime(uint64_t *idle_runtime_us)
{
    if (idle_runtime_us) {
        *idle_runtime_us = StCpuLocalP_GetData()->scheduler.idle_runtime_us;
    }

    return STATUS_SUCCESS;
}

void StScheduler_AccountIdleRuntime(uint64_t idle_delta_us)
{
    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;

    scheduler->idle_runtime_us += idle_delta_us;
}

int StScheduler_CheckHasOtherRunnableThread(void)
{
    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;
    struct StThread *current = scheduler->current_thread;
    struct StThread *next =
        select_next_runnable_thread(scheduler, StTimeP_GetUptimeNanoseconds() / 1000, 0);

    return next && next != current;
}

int StScheduler_ShouldMaintain(void)
{
    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;
    uint64_t switch_delta;

    ensure_scheduler_defaults(scheduler);

    if (scheduler->maintain_requested) {
        return 1;
    }

    switch_delta = scheduler->context_switch_count - scheduler->last_maintain_switch_count;
    if (switch_delta >= scheduler->maintain_interval_switches) {
        return 1;
    }

    return 0;
}

void StScheduler_RequestMaintain(void)
{
    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;
    scheduler->maintain_requested = 1;
}

StStatus StScheduler_Maintain(void)
{
    int unwait_thread;
    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;
    struct StThread *current;
    int needs_followup_maintain = 0;

    ensure_scheduler_defaults(scheduler);

    StThread_LockPreemption();

    /* check waiting threads */
    for (current = scheduler->runqueue_head; current; current = current->next) {
        if (current->state != THREAD_STATE_WAITING) continue;
        if (!current->wait_list) continue;

        unwait_thread = 1;

        for (int i = 0; i < current->wait_count; i++) {
            if (!current->wait_list[i]) continue;
            if (current->wait_list[i]->state != THREAD_STATE_FINISHED) {
                unwait_thread = 0;
                break;
            }

            current->wait_list[i] = NULL;
        }

        if (unwait_thread) {
            current->wait_list = NULL;
            current->state = THREAD_STATE_RUNNING;
            catch_up_runnable_pass(scheduler, current);
        }
    }

    /* remove finished detached threads */
    current = scheduler->runqueue_head;
    while (current) {
        struct StThread *next = current->next;

        if (current == scheduler->current_thread) {
            if (current->state == THREAD_STATE_FINISHED && current->is_detached) {
                /*
                 * A detached thread that is yielding out of StThread_Exit() is
                 * still executing on its own kernel stack here, so we cannot
                 * destroy it until after at least one more context switch.
                 */
                needs_followup_maintain = 1;
            }
            current = next;
            continue;
        }

        if (current->state == THREAD_STATE_FINISHED && current->is_detached) {
            StThread_Remove(current);

            /*
             * StThread_Remove mutates runqueue; restart traversal from head
             * to avoid using stale list links.
             */
            current = scheduler->runqueue_head;
            continue;
        }

        current = next;
    }

    scheduler->maintain_requested = needs_followup_maintain;
    scheduler->last_maintain_switch_count = scheduler->context_switch_count;

    StThread_UnlockPreemption();

    return STATUS_SUCCESS;
}
