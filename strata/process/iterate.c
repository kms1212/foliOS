#include "internal.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <strata/compiler.h>
#include <strata/gnt.h>
#include <strata/gnt_refs.h>
#include <strata/limits.h>
#include <strata/macros.h>
#include <strata/process.h>
#include <strata/process_refs.h>
#include <strata/ref_control.h>
#include <strata/status.h>
#include <strata/utf.h>

static StStatus format_id_name(
    StProcess_Id id, St_Utf32Char *name_out, size_t name_out_size, size_t *name_len_out
)
{
    char name_utf8[32];

    snprintf(name_utf8, sizeof(name_utf8), "%d", id);

    return StUtf_Utf8ToUtf32(
        (const St_Utf8Char *)name_utf8,
        strnlen(name_utf8, sizeof(name_utf8)),
        name_out,
        name_out_size,
        name_len_out
    );
}

static StGnt_Node_InternalRef find_registered_child(
    StGnt_Node_StrongRef parent, const St_Utf32Char *name, size_t name_len
)
{
    StGnt_Node_InternalRef child;

    if (!parent || parent->type == GNT_NODETYPE_LINK) return NULL;

    child = parent->children_head;
    while (child) {
        if (child->name_len == name_len &&
            memcmp(child->name, name, name_len * sizeof(*name)) == 0) {
            return child;
        }
        child = child->sibling;
    }

    return NULL;
}

static StStatus append_entry(
    uint8_t **buffer,
    size_t *remaining,
    size_t *entry_count,
    uint64_t cookie,
    uint16_t type,
    const St_Utf32Char *name,
    size_t name_len
)
{
    struct StGnt_DirectoryEntry *entry;
    size_t name_size;
    size_t entry_len;

    if (!buffer || !remaining || !entry_count) return STATUS_INVALID_VALUE;

    name_size = name_len * sizeof(*name);
    entry_len = ALIGN(offsetof(struct StGnt_DirectoryEntry, name) + name_size, sizeof(uint64_t));

    if (entry_len > *remaining) return STATUS_BUFFER_TOO_SMALL;
    if (!*buffer) return STATUS_BUFFER_TOO_SMALL;

    entry = (struct StGnt_DirectoryEntry *)*buffer;
    entry->cookie = cookie;
    entry->entry_len = entry_len;
    entry->name_len = name_len;
    entry->type = type;

    if (name_size) {
        memcpy(entry->name, name, name_size);
    }
    if (entry_len > offsetof(struct StGnt_DirectoryEntry, name) + name_size) {
        memset(
            (uint8_t *)entry + offsetof(struct StGnt_DirectoryEntry, name) + name_size,
            0,
            entry_len - (offsetof(struct StGnt_DirectoryEntry, name) + name_size)
        );
    }

    *buffer += entry_len;
    *remaining -= entry_len;
    (*entry_count)++;

    return STATUS_SUCCESS;
}

static StStatus iterate_process_root(
    uint64_t cookie, uint8_t *buffer, size_t buffer_size, size_t *entry_count, uint64_t *next_cookie
)
{
    StStatus status = STATUS_END_OF_LIST;
    StProcess_InternalRef process;
    size_t count = 0;
    uint64_t last_cookie = cookie;

    process = (StProcess_InternalRef)StProcess_GetListHead();
    while (process) {
        St_Utf32Char name[NODENAME_MAX];
        size_t name_len;

        if (!StRefControlBlock_IsDying(&process->ref_control) && !process->gnt_node &&
            (uint64_t)process->id > cookie) {
            status = format_id_name(process->id, name, sizeof(name), &name_len);
            if (!CHECK_SUCCESS(status)) return status;

            status = append_entry(
                &buffer,
                &buffer_size,
                &count,
                (uint64_t)process->id,
                GNT_NODETYPE_DIRECTORY,
                name,
                name_len
            );
            if (!CHECK_SUCCESS(status)) {
                if (count == 0) return status;
                status = STATUS_SUCCESS;
                break;
            }

            last_cookie = (uint64_t)process->id;
            status = STATUS_SUCCESS;
        }

        process = process->next;
    }

    *entry_count = count;
    *next_cookie = last_cookie;

    if (count == 0) return STATUS_END_OF_LIST;
    if (!process) return STATUS_END_OF_LIST;

    return status;
}

static StStatus iterate_process_directory(
    StGnt_Node_StrongRef parent,
    StProcess_BorrowedRef process,
    uint64_t cookie,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *entry_count,
    uint64_t *next_cookie
)
{
    static const struct {
        uint64_t cookie;
        const St_Utf32Char *name;
        size_t name_len;
        uint16_t type;
    } entries[] = {
        {1, U"Threads", 7, GNT_NODETYPE_DIRECTORY},
        {2, U"Stdin", 5, GNT_NODETYPE_LEAF},
        {3, U"Stdout", 6, GNT_NODETYPE_LEAF},
        {4, U"Stderr", 6, GNT_NODETYPE_LEAF},
    };

    StStatus status = STATUS_END_OF_LIST;
    size_t count = 0;
    uint64_t last_cookie = cookie;

    if (!process) {
        *entry_count = 0;
        *next_cookie = cookie;
        return STATUS_END_OF_LIST;
    }

    for (size_t i = 0; i < ARRAY_SIZE(entries); i++) {
        if (entries[i].cookie <= cookie) continue;
        if (find_registered_child(parent, entries[i].name, entries[i].name_len)) continue;

        status = append_entry(
            &buffer,
            &buffer_size,
            &count,
            entries[i].cookie,
            entries[i].type,
            entries[i].name,
            entries[i].name_len
        );
        if (!CHECK_SUCCESS(status)) {
            if (count == 0) return status;
            *entry_count = count;
            *next_cookie = last_cookie;
            return STATUS_SUCCESS;
        }

        last_cookie = entries[i].cookie;
        status = STATUS_SUCCESS;
    }

    *entry_count = count;
    *next_cookie = last_cookie;

    if (count == 0) return STATUS_END_OF_LIST;
    if (last_cookie == entries[ARRAY_SIZE(entries) - 1].cookie) return STATUS_END_OF_LIST;

    return status;
}

static StStatus iterate_threads_directory(
    StGnt_Node_StrongRef parent,
    StProcess_BorrowedRef process,
    uint64_t cookie,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *entry_count,
    uint64_t *next_cookie
)
{
    size_t count = 0;

    if (!process || !process->main_thread ||
        StRefControlBlock_IsDying(&process->main_thread->ref_control) || cookie >= 1) {
        *entry_count = 0;
        *next_cookie = cookie;
        return STATUS_END_OF_LIST;
    }

    if (find_registered_child(parent, U"Main", 4)) {
        *entry_count = 0;
        *next_cookie = cookie;
        return STATUS_END_OF_LIST;
    }

    if (CHECK_FAILURE(
            append_entry(&buffer, &buffer_size, &count, 1, GNT_NODETYPE_LEAF, U"Main", 4)
        )) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    *entry_count = count;
    *next_cookie = 1;

    return STATUS_END_OF_LIST;
}

StStatus StProcessGnt_Iterate(
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

    StStatus status;
    StProcess_BorrowedRef process;

    if (!parent) return STATUS_INVALID_VALUE;
    if (!buffer && buffer_size != 0) return STATUS_INVALID_VALUE;

    if (StProcessGnt_IsProcessRootNode((StGnt_Node_InternalRef)parent)) {
        return iterate_process_root(
            cookie,
            (uint8_t *)buffer,
            buffer_size,
            entry_count,
            next_cookie
        );
    }

    status = StProcessGnt_GetProcessFromNode(parent, &process);
    if (CHECK_SUCCESS(status)) {
        return iterate_process_directory(
            parent,
            process,
            cookie,
            (uint8_t *)buffer,
            buffer_size,
            entry_count,
            next_cookie
        );
    }
    if (status != STATUS_INVALID_HANDLE) return status;

    if (!parent->parent || parent->name_len != 7 ||
        memcmp(parent->name, U"Threads", 7 * sizeof(St_Utf32Char)) != 0) {
        *entry_count = 0;
        *next_cookie = cookie;
        return STATUS_NOT_A_DIRECTORY;
    }

    {
        status = StProcessGnt_GetProcessFromNode((StGnt_Node_StrongRef)parent->parent, &process);
        if (!CHECK_SUCCESS(status)) return status;
    }

    return iterate_threads_directory(
        parent,
        process,
        cookie,
        (uint8_t *)buffer,
        buffer_size,
        entry_count,
        next_cookie
    );
}
