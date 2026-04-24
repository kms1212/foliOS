#include <strata/handle.h>

#include <inttypes.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/log.h>
#include <strata/mm/pool.h>
#include <strata/status.h>
#include <strata/thread.h>

#define MODULE_NAME "handle"

static StStatus find_handle(
    struct StHandle_Table *table __in,
    StHandle_Id handle __in,
    struct StHandle_Entry **prev_out __out_optional,
    struct StHandle_Entry **entry_out __out_optional
)
{
    struct StHandle_Entry *prev = NULL;
    struct StHandle_Entry *current = table->head;

    while (current) {
        if (current->id == handle) {
            if (prev_out) *prev_out = prev;
            if (entry_out) *entry_out = current;

            return STATUS_SUCCESS;
        }

        prev = current;
        current = current->next;
    }

    return STATUS_INVALID_HANDLE;
}

static void retain_handle_object(enum StHandle_Type type, void *object)
{
    switch (type) {
    case ST_HANDLE_TYPE_GNT_NODE:
        StGnt_AcquireNode((struct StGnt_Node *)object);
        return;
    default:
        return;
    }
}

static void release_handle_object(enum StHandle_Type type, void *object)
{
    switch (type) {
    case ST_HANDLE_TYPE_GNT_NODE:
        StGnt_ReleaseNode((struct StGnt_Node *)object);
        return;
    default:
        return;
    }
}

void StHandle_ReleaseObject(enum StHandle_Type type, void *object)
{
    release_handle_object(type, object);
}

void StHandle_InitTable(struct StHandle_Table *table)
{
    if (!table) return;

    table->head = NULL;
    table->tail = NULL;
    table->next_id = 0;
}

StStatus StHandle_Create(
    struct StHandle_Table *table, enum StHandle_Type type, void *object, StHandle_Id *handle_out
)
{
    StStatus status;
    struct StHandle_Entry *new_entry = NULL;
    StHandle_Id start_handle, handle_id;
    int object_retained = 0;

    if (!table || !object) return STATUS_INVALID_VALUE;

    status = StPool_AllocateClear(sizeof(*new_entry), (void **)&new_entry);
    if (!CHECK_SUCCESS(status)) return status;

    retain_handle_object(type, object);
    object_retained = 1;

    StThread_LockPreemption();

    start_handle = table->next_id;
    handle_id = start_handle;
    for (;;) {
        status = find_handle(table, handle_id, NULL, NULL);
        if (status == STATUS_INVALID_HANDLE) break;
        if (!CHECK_SUCCESS(status)) goto has_error;

        handle_id++;
        if (handle_id == start_handle) {
            status = STATUS_TOO_MANY_OPEN_FILES;
            goto has_error;
        }
    }

    new_entry->id = handle_id;
    new_entry->type = type;
    new_entry->object = object;

    if (!table->head) {
        table->head = table->tail = new_entry;
    } else {
        table->tail->next = new_entry;
        table->tail = new_entry;
    }

    table->next_id = handle_id + 1;

    StThread_UnlockPreemption();

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "created handle %" PRIu32 " (type=%d)\n", handle_id, type);

    if (handle_out) *handle_out = handle_id;

    return STATUS_SUCCESS;

has_error:
    StThread_UnlockPreemption();
    if (object_retained) release_handle_object(type, object);
    StPool_Free(new_entry);

    return status;
}

StStatus StHandle_Get(
    struct StHandle_Table *table,
    StHandle_Id handle,
    enum StHandle_Type *type_out,
    void **object_out
)
{
    StStatus status;
    struct StHandle_Entry *entry;
    enum StHandle_Type type;
    void *object;

    if (!table) return STATUS_INVALID_VALUE;

    StThread_LockPreemption();
    status = find_handle(table, handle, NULL, &entry);
    if (!CHECK_SUCCESS(status)) {
        StThread_UnlockPreemption();
        return status;
    }

    type = entry->type;
    object = entry->object;
    StThread_UnlockPreemption();

    if (type_out) *type_out = type;
    if (object_out) *object_out = object;

    return STATUS_SUCCESS;
}

StStatus StHandle_GetRetained(
    struct StHandle_Table *table,
    StHandle_Id handle,
    enum StHandle_Type *type_out,
    void **object_out
)
{
    StStatus status;
    struct StHandle_Entry *entry;
    enum StHandle_Type type;
    void *object;

    if (!table) return STATUS_INVALID_VALUE;

    StThread_LockPreemption();
    status = find_handle(table, handle, NULL, &entry);
    if (!CHECK_SUCCESS(status)) {
        StThread_UnlockPreemption();
        return status;
    }

    type = entry->type;
    object = entry->object;
    retain_handle_object(type, object);
    StThread_UnlockPreemption();

    if (type_out) *type_out = type;
    if (object_out) *object_out = object;

    return STATUS_SUCCESS;
}

StStatus StHandle_Close(struct StHandle_Table *table, StHandle_Id handle)
{
    StStatus status;
    struct StHandle_Entry *prev_entry = NULL;
    struct StHandle_Entry *entry = NULL;
    enum StHandle_Type type;
    void *object;

    if (!table) return STATUS_INVALID_VALUE;

    StThread_LockPreemption();

    status = find_handle(table, handle, &prev_entry, &entry);
    if (!CHECK_SUCCESS(status)) {
        StThread_UnlockPreemption();
        return status;
    }

    if (prev_entry) {
        prev_entry->next = entry->next;
    } else {
        table->head = entry->next;
    }

    if (table->tail == entry) {
        table->tail = prev_entry;
    }

    type = entry->type;
    object = entry->object;
    entry->next = NULL;

    StThread_UnlockPreemption();

    release_handle_object(type, object);

    LOG_DEBUG(LM_CAT_UNCLASSIFIED, "closed handle %" PRIu32 "\n", handle);

    StPool_Free(entry);

    return STATUS_SUCCESS;
}

void StHandle_ClearTable(struct StHandle_Table *table)
{
    struct StHandle_Entry *current;

    if (!table) return;

    StThread_LockPreemption();

    current = table->head;
    table->head = NULL;
    table->tail = NULL;
    table->next_id = 0;

    StThread_UnlockPreemption();

    while (current) {
        struct StHandle_Entry *next = current->next;

        release_handle_object(current->type, current->object);
        StPool_Free(current);
        current = next;
    }
}
