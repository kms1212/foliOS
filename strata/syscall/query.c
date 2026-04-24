#include <strata/syscall.h>

#include <inttypes.h>
#include <stdint.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/gnt/interface.h>
#include <strata/handle.h>
#include <strata/log.h>
#include <strata/process.h>
#include <strata/scheduler.h>
#include <strata/status.h>
#include <strata/thread.h>
#include <strata/uuid.h>

#define MODULE_NAME "syscall"

static void log_uuid_hex(const char *label, const struct StUuid *uuid)
{
    if (!uuid) {
        LOG_DEBUG(LM_CAT_UNCLASSIFIED, "%s <null>\n", label);
        return;
    }

    LOG_DEBUG(
        LM_CAT_UNCLASSIFIED,
        "%s %02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
        label,
        uuid->data[0],
        uuid->data[1],
        uuid->data[2],
        uuid->data[3],
        uuid->data[4],
        uuid->data[5],
        uuid->data[6],
        uuid->data[7],
        uuid->data[8],
        uuid->data[9],
        uuid->data[10],
        uuid->data[11],
        uuid->data[12],
        uuid->data[13],
        uuid->data[14],
        uuid->data[15]
    );
}

static StStatus get_current_process(struct StProcess **process_out)
{
    StStatus status;
    struct StThread *thread;

    status = StScheduler_GetCurrentThread(&thread);
    if (!CHECK_SUCCESS(status)) return status;
    if (!thread || !thread->process) return STATUS_INVALID_THREAD;

    if (process_out) *process_out = thread->process;

    return STATUS_SUCCESS;
}
StStatus StSyscall_Query(
    uint32_t handle __in,
    const struct StUuid *if_uuid __in,
    uint32_t request_abiver __in,
    uint32_t *funcid_base __out,
    uint32_t *result_abiver __out
)
{
    StStatus status;
    struct StProcess *process;
    struct StGnt_Node *node;
    enum StHandle_Type handle_type;
    int node_retained = 0;

    status = get_current_process(&process);
    if (!CHECK_SUCCESS(status)) return status;

    status = StHandle_GetRetained(&process->handle_table, handle, &handle_type, (void **)&node);
    if (!CHECK_SUCCESS(status)) return status;
    if (handle_type != ST_HANDLE_TYPE_GNT_NODE) {
        StHandle_ReleaseObject(handle_type, node);
        return STATUS_INVALID_HANDLE;
    }
    node_retained = 1;

    status = StGnt_QueryInterface(node, if_uuid, request_abiver, funcid_base, result_abiver);
    if (!CHECK_SUCCESS(status)) {
        if (status == STATUS_NOT_SUPPORTED) {
            uint32_t entry_base = 0;
            struct StGnt_NodeInterface *entry = node->interface_head;

            LOG_DEBUG(
                LM_CAT_UNCLASSIFIED,
                "query miss: handle %" PRIu32 ", request abi %" PRIu32 "\n",
                handle,
                request_abiver
            );

            while (entry) {
                LOG_DEBUG(
                    LM_CAT_UNCLASSIFIED,
                    "query available: base=%" PRIu32 ", abi=%" PRIu32 ", span=%" PRIu32 "\n",
                    entry_base,
                    entry->abi_version,
                    entry->funcid_span
                );
                log_uuid_hex("query available uuid:", &entry->uuid);

                entry_base += entry->funcid_span;
                entry = entry->next;
            }
        }

        goto done;
    }

    LOG_DEBUG(
        LM_CAT_UNCLASSIFIED,
        "query: handle %" PRIu32
        ", interface %02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X, abi "
        "%" PRIu32 " -> base %" PRIu32 ", abi %" PRIu32 "\n",
        handle,
        if_uuid->data[0],
        if_uuid->data[1],
        if_uuid->data[2],
        if_uuid->data[3],
        if_uuid->data[4],
        if_uuid->data[5],
        if_uuid->data[6],
        if_uuid->data[7],
        if_uuid->data[8],
        if_uuid->data[9],
        if_uuid->data[10],
        if_uuid->data[11],
        if_uuid->data[12],
        if_uuid->data[13],
        if_uuid->data[14],
        if_uuid->data[15],
        request_abiver,
        funcid_base ? *funcid_base : 0,
        result_abiver ? *result_abiver : 0
    );

    status = STATUS_SUCCESS;

done:
    if (node_retained) {
        StGnt_ReleaseNode(node);
    }

    return status;
}
