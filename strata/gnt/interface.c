#include <strata/gnt/interface.h>

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/gnt_refs.h>
#include <strata/mm/pool.h>
#include <strata/status.h>
#include <strata/uuid.h>

const struct StUuid StGntIf_Uuid_ByteStream = {
    .data = {
        0xCB,
        0xBC,
        0x8C,
        0xA1,
        0x65,
        0x84,
        0x54,
        0xA5,
        0x99,
        0x91,
        0x09,
        0xEB,
        0x5D,
        0xC7,
        0xD4,
        0x0D,
    },
};

const struct StUuid StGntIf_Uuid_Directory = {
    .data = {
        0x6E,
        0xC9,
        0x31,
        0x9C,
        0xA6,
        0x05,
        0x53,
        0x04,
        0xA7,
        0x0F,
        0x20,
        0x69,
        0x71,
        0x1F,
        0xEF,
        0xEE,
    },
};

const struct StUuid StGntIf_Uuid_FileInfo = {
    .data = {
        0xBA,
        0x2A,
        0x97,
        0x7D,
        0x08,
        0x6A,
        0x55,
        0x93,
        0xAF,
        0xD5,
        0x9A,
        0xBC,
        0x90,
        0xE2,
        0x73,
        0xAD,
    },
};

const struct StUuid StGntIf_Uuid_Process = {
    .data = {
        0x12,
        0xD2,
        0xE5,
        0xAE,
        0xB0,
        0x8B,
        0x5B,
        0x7F,
        0xA9,
        0x7B,
        0x87,
        0xB7,
        0xC4,
        0x33,
        0x4B,
        0x26,
    },
};

const struct StUuid StGntIf_Uuid_Thread = {
    .data = {
        0x11,
        0x0C,
        0x52,
        0xCF,
        0x19,
        0xE1,
        0x58,
        0xF6,
        0xA5,
        0x26,
        0x67,
        0x0A,
        0x2F,
        0x4D,
        0x88,
        0xC2,
    },
};

static int uuid_equals(const struct StUuid *lhs, const struct StUuid *rhs)
{
    if (!lhs || !rhs) return 0;

    return memcmp(lhs, rhs, sizeof(*lhs)) == 0;
}

StStatus StGnt_RegisterInterface(
    StGnt_Node_StrongRef node __inout,
    const struct StUuid *if_uuid __in,
    uint32_t abi_version __in,
    uint32_t funcid_span __in
)
{
    assert(node);

    StStatus status;
    struct StGnt_NodeInterface *entry;

    if (!if_uuid || !funcid_span) return STATUS_INVALID_VALUE;

    entry = node->interface_head;
    while (entry) {
        if (uuid_equals(&entry->uuid, if_uuid) && entry->abi_version == abi_version) {
            if (entry->funcid_span != funcid_span) return STATUS_CONFLICTING_STATE;
            return STATUS_SUCCESS;
        }

        entry = entry->next;
    }

    status = StPool_AllocateClear(sizeof(*entry), (void **)&entry);
    if (!CHECK_SUCCESS(status)) return status;

    memcpy(&entry->uuid, if_uuid, sizeof(entry->uuid));
    entry->abi_version = abi_version;
    entry->funcid_span = funcid_span;

    if (!node->interface_head) {
        node->interface_head = node->interface_tail = entry;
    } else {
        node->interface_tail->next = entry;
        node->interface_tail = entry;
    }

    return STATUS_SUCCESS;
}

StStatus StGnt_QueryInterface(
    StGnt_Node_StrongRef node __in,
    const struct StUuid *if_uuid __in,
    uint32_t request_abiver __in,
    uint32_t *funcid_base_out __out_optional,
    uint32_t *result_abiver_out __out_optional
)
{
    uint32_t funcid_base = 0;
    uint32_t matched_base = 0;
    uint32_t matched_abiver = 0;
    int found = 0;
    struct StGnt_NodeInterface *entry;

    if (!node || !if_uuid) return STATUS_INVALID_VALUE;

    entry = node->interface_head;
    while (entry) {
        if (uuid_equals(&entry->uuid, if_uuid) && entry->abi_version <= request_abiver &&
            (!found || entry->abi_version > matched_abiver)) {
            matched_base = funcid_base;
            matched_abiver = entry->abi_version;
            found = 1;
        }

        funcid_base += entry->funcid_span;
        entry = entry->next;
    }

    if (!found) return STATUS_NOT_SUPPORTED;

    if (funcid_base_out) *funcid_base_out = matched_base;
    if (result_abiver_out) *result_abiver_out = matched_abiver;

    return STATUS_SUCCESS;
}
