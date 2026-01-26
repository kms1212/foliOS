#include <strata/interrupt.h>

#include <stdio.h>
#include <stdlib.h>

#include <strata/compiler.h>
#include <strata/log.h>
#include <strata/macros.h>
#include <strata/panic.h>

#define MODULE_NAME "irq"

StStatus StInt_CreateHandler(
    int num, void *data, StInt_HandlerFunction func, struct StInt_Handler **handler
)
{
    StStatus status;
    struct StInt_Handler *newentry = NULL;
    struct StInt_Handler *firstentry;

    if (num > 0xFF) {
        status = STATUS_INVALID_VALUE;
        goto has_error;
    }

    LOG_DEBUG("adding intrrupt handler to #%02X...\n", num);

    newentry = malloc(sizeof(*newentry));
    if (!newentry) {
        status = STATUS_UNKNOWN_ERROR;
        goto has_error;
    }
    newentry->next = NULL;
    newentry->data = data;
    newentry->handler = func;
    newentry->irq_num = num;

    status = StIntP_GetFirstHandler(num, &firstentry);
    if (!CHECK_SUCCESS(status)) goto has_error;
    if (!firstentry) {
        status = StIntP_SetFirstHandler(num, newentry);
        if (!CHECK_SUCCESS(status)) goto has_error;
    } else {
        struct StInt_Handler *entry = firstentry;
        for (; entry->next; entry = entry->next) {
        }

        entry->next = newentry;
    }

    if (handler) *handler = newentry;

    return STATUS_SUCCESS;

has_error:
    if (newentry) {
        free(newentry);
    }

    return status;
}

void StInt_RemoveHandler(struct StInt_Handler *handler)
{
    StStatus status;
    struct StInt_Handler *prev = NULL;
    struct StInt_Handler *current;

    LOG_DEBUG("removing intrrupt handler from #%02X...\n", handler->irq_num);

    status = StIntP_GetFirstHandler(handler->irq_num, &current);
    if (!CHECK_SUCCESS(status)) return;
    if (!current) return;

    if (current == handler) {
        status = StIntP_SetFirstHandler(handler->irq_num, handler->next);
        if (!CHECK_SUCCESS(status)) return;
    } else {
        for (; current->next; current = current->next) {
            if (current->next == handler) {
                prev = current;
            }
        }
        if (!prev) return;

        prev->next = handler->next;
    }

    free(handler);
}
