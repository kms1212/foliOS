#include <strata/handle.h>

#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/gnt/interface.h>
#include <strata/limits.h>
#include <strata/log.h>
#include <strata/mm/pool.h>
#include <strata/module.h>
#include <strata/process.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/syscall.h>
#include <strata/thread.h>
#include <strata/utf.h>

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

void StHandle_TableReleaseObject(enum StHandle_Type type, void *object)
{
    release_handle_object(type, object);
}

void StHandle_TableInit(struct StHandle_Table *table)
{
    if (!table) return;

    table->head = NULL;
    table->tail = NULL;
    table->next_id = 0;
}

StStatus StHandle_TableCreate(
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

StStatus StHandle_TableGet(
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

StStatus StHandle_TableGetRetained(
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

StStatus StHandle_TableClose(struct StHandle_Table *table, StHandle_Id handle)
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

void StHandle_TableClear(struct StHandle_Table *table)
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

static struct StHandle_Table kernel_handle_table;
static int kernel_handle_table_initialized = 0;

static struct StHandle_Table *get_kernel_handle_table(void)
{
    if (!kernel_handle_table_initialized) {
        StHandle_TableInit(&kernel_handle_table);
        kernel_handle_table_initialized = 1;
    }

    return &kernel_handle_table;
}

static StStatus get_current_handle_table(struct StHandle_Table **table_out)
{
    StStatus status;
    struct StThread *thread = NULL;

    if (!table_out) return STATUS_INVALID_VALUE;

    status = StScheduler_GetCurrentThread(&thread);
    if (CHECK_SUCCESS(status) && thread && thread->process) {
        *table_out = &thread->process->handle_table;
        return STATUS_SUCCESS;
    }

    *table_out = get_kernel_handle_table();

    return STATUS_SUCCESS;
}

static StStatus get_node_from_handle(StHandle handle, struct StGnt_Node **node_out)
{
    StStatus status;
    struct StHandle_Table *table;
    struct StGnt_Node *node;
    enum StHandle_Type type;

    status = get_current_handle_table(&table);
    if (!CHECK_SUCCESS(status)) return status;

    status = StHandle_TableGetRetained(table, (StHandle_Id)handle, &type, (void **)&node);
    if (!CHECK_SUCCESS(status)) return status;
    if (type != ST_HANDLE_TYPE_GNT_NODE) {
        StHandle_TableReleaseObject(type, node);
        return STATUS_INVALID_HANDLE;
    }

    if (node_out) *node_out = node;

    return STATUS_SUCCESS;
}

StStatus StHandle_Open(const uint8_t *path, uint32_t flags, StHandle *handle)
{
    size_t path_len;
    St_Utf32Char path_buf[PATH_MAX];
    StStatus status;
    struct StHandle_Table *table;
    struct StGnt_Node *node;
    StHandle_Id new_handle;

    if (!path) return STATUS_INVALID_VALUE;

    path_len = strnlen((const char *)path, PATH_UTF8_MAX);
    status = StUtf_Utf8ToUtf32(path, path_len, path_buf, sizeof(path_buf), NULL);
    if (!CHECK_SUCCESS(status)) return status;

    status = StGnt_ResolvePath(g_gnt_root_local, path_buf, &node);
    if (!CHECK_SUCCESS(status)) return status;

    status = get_current_handle_table(&table);
    if (!CHECK_SUCCESS(status)) return status;

    status = StHandle_TableCreate(table, ST_HANDLE_TYPE_GNT_NODE, node, &new_handle);
    if (!CHECK_SUCCESS(status)) return status;

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "path: %s, flags: %08" PRIX32 " -> handle %" PRIu32 "\n",
        (const char *)path,
        flags,
        new_handle
    );

    if (handle) *handle = (StHandle)new_handle;

    return STATUS_SUCCESS;
}

StStatus StHandle_Close(StHandle handle)
{
    StStatus status;
    struct StHandle_Table *table;

    status = get_current_handle_table(&table);
    if (!CHECK_SUCCESS(status)) return status;

    return StHandle_TableClose(table, (StHandle_Id)handle);
}

StStatus StHandle_Query(
    StHandle handle,
    const struct StUuid *if_uuid,
    uint32_t request_abiver,
    uint32_t *funcid_base,
    uint32_t *result_abiver
)
{
    StStatus status;
    struct StGnt_Node *node;

    status = get_node_from_handle(handle, &node);
    if (!CHECK_SUCCESS(status)) return status;

    status = StGnt_QueryInterface(node, if_uuid, request_abiver, funcid_base, result_abiver);
    if (!CHECK_SUCCESS(status)) {
        if (status == STATUS_NOT_SUPPORTED) {
            uint32_t entry_base = 0;
            struct StGnt_NodeInterface *entry = node->interface_head;

            LOG_TRACE(
                LM_CAT_UNCLASSIFIED,
                "query miss: handle %" PRIu32 ", request abi %" PRIu32 "\n",
                (StHandle_Id)handle,
                request_abiver
            );

            while (entry) {
                LOG_TRACE(
                    LM_CAT_UNCLASSIFIED,
                    "query available: base=%" PRIu32 ", abi=%" PRIu32 ", span=%" PRIu32 "\n",
                    entry_base,
                    entry->abi_version,
                    entry->funcid_span
                );
                LOG_TRACE(LM_CAT_UNCLASSIFIED, "query available uuid: %pU\n", (void *)&entry->uuid);

                entry_base += entry->funcid_span;
                entry = entry->next;
            }
        }

        StGnt_ReleaseNode(node);
        return status;
    }

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "query: handle %" PRIu32 ", interface %pU, abi %" PRIu32 " -> base %" PRIu32
        ", abi %" PRIu32 "\n",
        (StHandle_Id)handle,
        (void *)if_uuid,
        request_abiver,
        funcid_base ? *funcid_base : 0,
        result_abiver ? *result_abiver : 0
    );

    StGnt_ReleaseNode(node);

    return STATUS_SUCCESS;
}

static void build_reg_args(
    unsigned long arg0, unsigned long arg1, unsigned long arg2, unsigned long arg3, long args_out[4]
)
{
    args_out[0] = (long)arg0;
    args_out[1] = (long)arg1;
    args_out[2] = (long)arg2;
    args_out[3] = (long)arg3;
}

StStatus StHandle_CallReg(
    StHandle handle,
    uint32_t funcid,
    unsigned long arg0,
    unsigned long arg1,
    unsigned long arg2,
    unsigned long arg3
)
{
    StStatus status;
    struct StGnt_Node *node;
    struct StModule *handler_module;
    int handled = 0;
    long args[4];

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "call_reg: handle %" PRIu32 ", funcid %" PRIu32 "\n",
        (StHandle_Id)handle,
        funcid
    );

    status = get_node_from_handle(handle, &node);
    if (!CHECK_SUCCESS(status)) return status;

    status = StSyscallA_DispatchCallReg(node, funcid, arg0, arg1, arg2, arg3, &handled);
    if (handled) {
        StGnt_ReleaseNode(node);
        return status;
    }

    build_reg_args(arg0, arg1, arg2, arg3, args);

    handler_module = node->handler_module;
    if (!handler_module || !handler_module->dispatch_args) {
        StGnt_ReleaseNode(node);
        return STATUS_NOT_SUPPORTED;
    }

    status = handler_module->dispatch_args(node, handle, funcid, args);
    StGnt_ReleaseNode(node);

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "call_reg result: handle %" PRIu32 ", funcid %" PRIu32 " -> %08" PRIX32 "\n",
        (StHandle_Id)handle,
        funcid,
        status
    );

    return status;
}

StStatus StHandle_CallPtr(
    StHandle handle,
    uint32_t funcid,
    const void *args,
    void *result,
    unsigned long arg0,
    unsigned long arg1
)
{
    StStatus status;
    struct StGnt_Node *node;
    struct StModule *handler_module;
    int handled = 0;
    long dispatch_args[4];

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "call_ptr: handle %" PRIu32 ", funcid %" PRIu32 "\n",
        (StHandle_Id)handle,
        funcid
    );

    status = get_node_from_handle(handle, &node);
    if (!CHECK_SUCCESS(status)) return status;

    status = StSyscallA_DispatchCallPtr(node, funcid, args, result, arg0, arg1, &handled);
    if (handled) {
        StGnt_ReleaseNode(node);
        return status;
    }

    dispatch_args[0] = (long)(uintptr_t)args;
    dispatch_args[1] = (long)(uintptr_t)result;
    dispatch_args[2] = (long)arg0;
    dispatch_args[3] = (long)arg1;

    handler_module = node->handler_module;
    if (!handler_module || !handler_module->dispatch_args) {
        StGnt_ReleaseNode(node);
        return STATUS_NOT_SUPPORTED;
    }

    status = handler_module->dispatch_args(node, handle, funcid, dispatch_args);
    StGnt_ReleaseNode(node);

    LOG_TRACE(
        LM_CAT_UNCLASSIFIED,
        "call_ptr result: handle %" PRIu32 ", funcid %" PRIu32 " -> %08" PRIX32 "\n",
        (StHandle_Id)handle,
        funcid,
        status
    );

    return status;
}

StStatus StHandle_Call0(StHandle handle, uint32_t funcid)
{
    return StHandle_CallReg(handle, funcid, 0, 0, 0, 0);
}

StStatus StHandle_Call1(StHandle handle, uint32_t funcid, unsigned long arg0)
{
    return StHandle_CallReg(handle, funcid, arg0, 0, 0, 0);
}

StStatus StHandle_Call2(StHandle handle, uint32_t funcid, unsigned long arg0, unsigned long arg1)
{
    return StHandle_CallReg(handle, funcid, arg0, arg1, 0, 0);
}

StStatus StHandle_Call3(
    StHandle handle, uint32_t funcid, unsigned long arg0, unsigned long arg1, unsigned long arg2
)
{
    return StHandle_CallReg(handle, funcid, arg0, arg1, arg2, 0);
}

StStatus StHandle_Call4(
    StHandle handle,
    uint32_t funcid,
    unsigned long arg0,
    unsigned long arg1,
    unsigned long arg2,
    unsigned long arg3
)
{
    return StHandle_CallReg(handle, funcid, arg0, arg1, arg2, arg3);
}

StStatus StHandle_CallN(
    StHandle handle,
    uint32_t funcid,
    const void *args,
    void *result,
    unsigned long arg0,
    unsigned long arg1
)
{
    return StHandle_CallPtr(handle, funcid, args, result, arg0, arg1);
}
