#include <strata/gnt.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <strata/compiler.h>
#include <strata/gnt_refs.h>
#include <strata/macros.h>
#include <strata/module.h>
#include <strata/status.h>

#define MODULE_COOKIE_FLAG ((uint64_t)1)

static StGnt_Node_InternalRef find_next_child(
    StGnt_Node_StrongRef parent __in, uint64_t cookie __in, StStatus *status __out
)
{
    assert(status);

    StGnt_Node_InternalRef child;

    if (cookie == 0) {
        *status = STATUS_SUCCESS;
        return parent->children_head;
    }

    child = parent->children_head;
    while (child) {
        if ((uint64_t)(uintptr_t)child == cookie) {
            *status = STATUS_SUCCESS;
            return child->sibling;
        }

        child = child->sibling;
    }

    *status = STATUS_ENTRY_NOT_FOUND;
    return NULL;
}

static int is_module_cookie(uint64_t cookie)
{
    return (cookie & MODULE_COOKIE_FLAG) != 0;
}

static uint64_t encode_module_cookie(uint64_t cookie)
{
    return (cookie << 1) | MODULE_COOKIE_FLAG;
}

static uint64_t decode_module_cookie(uint64_t cookie)
{
    return cookie >> 1;
}

StStatus StGnt_Iterate(
    StGnt_Node_StrongRef parent __in,
    uint64_t cookie __in,
    void *buffer __in,
    size_t buffer_size __in,
    size_t *entry_count __out,
    uint64_t *next_cookie __out
)
{
    assert(entry_count);
    assert(next_cookie);

    struct StModule *handler_module;
    StGnt_Node_InternalRef child;
    uint8_t *write_ptr = buffer;
    size_t remaining = buffer_size;
    size_t written_count = 0;
    StStatus status;
    size_t module_entry_count;
    uint64_t module_next_cookie;

    if (!parent || parent->type != GNT_NODETYPE_DIRECTORY) return STATUS_NOT_A_DIRECTORY;
    if (!buffer && buffer_size != 0) return STATUS_INVALID_VALUE;
    handler_module = parent->handler_module;
    *entry_count = 0;
    *next_cookie = cookie;

    if (is_module_cookie(cookie)) {
        if (!handler_module || !handler_module->list) return STATUS_INVALID_VALUE;

        status = handler_module->list(
            parent,
            decode_module_cookie(cookie),
            buffer,
            buffer_size,
            entry_count,
            &module_next_cookie
        );
        if (*entry_count > 0) {
            *next_cookie = encode_module_cookie(module_next_cookie);
        }

        return status;
    }

    child = find_next_child(parent, cookie, &status);
    if (!CHECK_SUCCESS(status)) return status;

    while (child) {
        struct StGnt_DirectoryEntry *entry;
        size_t entry_len;
        size_t name_size;

        name_size = child->name_len * sizeof(*child->name);
        entry_len =
            ALIGN(offsetof(struct StGnt_DirectoryEntry, name) + name_size, sizeof(uint64_t));

        if (entry_len > remaining) {
            if (written_count == 0) return STATUS_BUFFER_TOO_SMALL;
            break;
        }
        if (!write_ptr) {
            if (written_count == 0) return STATUS_BUFFER_TOO_SMALL;
            break;
        }

        entry = (struct StGnt_DirectoryEntry *)write_ptr;
        entry->cookie = (uint64_t)(uintptr_t)child;
        entry->entry_len = entry_len;
        entry->name_len = child->name_len;
        entry->type = child->type;

        if (name_size) {
            memcpy(entry->name, child->name, name_size);
        }
        if (entry_len > offsetof(struct StGnt_DirectoryEntry, name) + name_size) {
            memset(
                (uint8_t *)entry + offsetof(struct StGnt_DirectoryEntry, name) + name_size,
                0,
                entry_len - (offsetof(struct StGnt_DirectoryEntry, name) + name_size)
            );
        }

        write_ptr += entry_len;
        remaining -= entry_len;
        written_count++;
        *next_cookie = entry->cookie;

        child = child->sibling;
    }

    *entry_count = written_count;

    if (!child) {
        if (!handler_module || !handler_module->list) return STATUS_END_OF_LIST;

        module_entry_count = 0;
        module_next_cookie = 0;
        status =
            handler_module
                ->list(parent, 0, write_ptr, remaining, &module_entry_count, &module_next_cookie);
        if (module_entry_count > 0) {
            *entry_count += module_entry_count;
            *next_cookie = encode_module_cookie(module_next_cookie);
            return status;
        }

        if (status == STATUS_BUFFER_TOO_SMALL && written_count > 0) {
            *next_cookie = encode_module_cookie(0);
            return STATUS_SUCCESS;
        }

        return status;
    }

    return STATUS_SUCCESS;
}
