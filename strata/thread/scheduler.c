#include <strata/scheduler.h>

#include <strata/plat/cpulocal.h>
#include <strata/plat/scheduler.h>
#include <strata/plat/time.h>

#include <strata/log.h>
#include <strata/panic.h>

#define MODULE_NAME "sched"

StStatus StScheduler_AddThread(struct StThread *th)
{
    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;

    if (!scheduler->queue_head) {
        scheduler->queue_head = th;
        scheduler->queue_tail = th;
    } else {
        scheduler->queue_tail->next = th;
        scheduler->queue_tail = th;
    }
    th->next = NULL;

    LOG_DEBUG("thread #%d added to scheduler\n", th->id);

    return STATUS_SUCCESS;
}

StStatus StScheduler_RemoveThread(struct StThread *th)
{
    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;

    if (th->status != THREAD_STATE_FINISHED) {
        return STATUS_THREAD_NOT_FINISHED;
    }

    if (th == scheduler->queue_head) {
        scheduler->queue_head = th->next;
        if (!scheduler->queue_head) {
            scheduler->queue_tail = NULL;
        }
    }

    for (struct StThread *current = scheduler->queue_head; current->next; current = current->next) {
        if (th == current->next) {
            current->next = th->next;
            if (th == scheduler->queue_tail) {
                scheduler->queue_tail = current;
            }
            break;
        }
    }

    LOG_DEBUG("thread #%d removed from scheduler\n", th->id);

    return STATUS_SUCCESS;
}

StStatus StScheduler_GetCurrentThread(struct StThread **current)
{
    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;

    if (current) *current = scheduler->current;

    return STATUS_SUCCESS;
}

StStatus StScheduler_GetNextThread(struct StThread **next)
{
    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;

    struct StThread *next_thread = scheduler->current;

    do {
        next_thread = next_thread->next;
        if (!next_thread) {
            next_thread = scheduler->queue_head;
        }

        switch (next_thread->status) {
        case THREAD_STATE_RUNNING:
            break;
        case THREAD_STATE_PENDING:
            next_thread->status = THREAD_STATE_RUNNING;
            break;
        case THREAD_STATE_SLEEPING:
            if (StTimeP_GetGlobalTick() >= next_thread->sleep_until_tick) {
                next_thread->status = THREAD_STATE_RUNNING;
            }
            break;
        default:
            break;
        }

        if (next_thread->status == THREAD_STATE_RUNNING) {
            break;
        }
    } while (next_thread != scheduler->current);

    if (next) *next = next_thread;

    return STATUS_SUCCESS;
}

StStatus StScheduler_SetCurrentThread(struct StThread *th)
{
    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;

    scheduler->current = th;

    return STATUS_SUCCESS;
}

int StScheduler_CheckHasOtherRunnableThread(void)
{
    int result = 0;

    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;

    for (struct StThread *current = scheduler->queue_head; current; current = current->next) {
        if (current == scheduler->current) continue;

        if (current->status == THREAD_STATE_RUNNING || current->status == THREAD_STATE_PENDING) {
            result = 1;
            break;
        }
    }

    return result;
}

StStatus StScheduler_Maintain(void)
{
    int unwait_thread;
    struct StScheduler_Data *scheduler = &StCpuLocalP_GetData()->scheduler;
    struct StThread *current, *prev;

    if (scheduler->current && scheduler->current->type != THREAD_TYPE_MAIN) {
        /* only the main thread can call this function */
        return STATUS_INVALID_THREAD;
    }

    for (current = scheduler->queue_head; current; current = current->next) {
        if (current->status != THREAD_STATE_WAITING) continue;
        if (!current->wait_list) continue;

        unwait_thread = 1;

        for (int i = 0; i < current->wait_count; i++) {
            if (!current->wait_list[i]) continue;
            if (current->wait_list[i]->status != THREAD_STATE_FINISHED) {
                unwait_thread = 0;
                break;
            }

            current->wait_list[i] = NULL;
        }

        if (unwait_thread) {
            current->wait_list = NULL;
            current->status = THREAD_STATE_RUNNING;
        }
    }

    prev = NULL;
    current = scheduler->queue_head;
    while (current) {
        struct StThread *thread_to_remove;

        if (current->status != THREAD_STATE_FINISHED) {
            prev = current;
            current = current->next;
            continue;
        }

        thread_to_remove = current;

        if (prev) {
            prev->next = current->next;
            if (thread_to_remove == scheduler->queue_tail) {
                scheduler->queue_tail = prev;
            }
        } else {
            scheduler->queue_head = current->next;
        }

        current = current->next;

        LOG_DEBUG("thread #%d removed from scheduler\n", thread_to_remove->id);

        if (thread_to_remove->is_detached) {
            StThread_Remove(thread_to_remove);
        }
    }

    return STATUS_SUCCESS;
}
