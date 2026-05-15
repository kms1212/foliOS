#include <strata/syscall.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <strata/gnt.h>
#include <strata/gnt_refs.h>
#include <strata/status.h>
#include <strata/utf.h>

#define BS_FUNCID_SEEK      0
#define BS_FUNCID_TELL      1
#define BS_FUNCID_READ      2
#define BS_FUNCID_WRITE     3
#define BS_FUNCID_SYNC      4
#define BS_FUNCID_GETLENGTH 5

#define STDIO_DEBUGCON_PORT ((uint16_t)0x00E9)

enum stdio_node_kind {
    STDIO_NODE_NONE = 0,
    STDIO_NODE_STDIN,
    STDIO_NODE_STDOUT,
    STDIO_NODE_STDERR,
};

static enum stdio_node_kind get_stdio_node_kind(const struct StGnt_Node *node)
{
    if (!node || !node->parent || node->parent->parent != g_gnt_system_processes) {
        return STDIO_NODE_NONE;
    }

    if (node->name_len == 5 &&
        StUtf_CompareUtf32Chars(node->name, node->name_len, U"Stdin", 5) == 0) {
        return STDIO_NODE_STDIN;
    }
    if (node->name_len == 6 &&
        StUtf_CompareUtf32Chars(node->name, node->name_len, U"Stdout", 6) == 0) {
        return STDIO_NODE_STDOUT;
    }
    if (node->name_len == 6 &&
        StUtf_CompareUtf32Chars(node->name, node->name_len, U"Stderr", 6) == 0) {
        return STDIO_NODE_STDERR;
    }

    return STDIO_NODE_NONE;
}

extern int early_print_char(void *_state, char ch);
extern struct print_state pstate;

static void stdio_debugcon_write(const uint8_t *buf, uint64_t size)
{
    for (uint64_t i = 0; i < size; i++) {
        early_print_char(&pstate, (char)buf[i]);
    }
}

StStatus StSyscallA_DispatchCallReg(
    StGnt_Node_StrongRef node,
    uint32_t funcid,
    unsigned long arg0,
    unsigned long arg1,
    unsigned long arg2,
    unsigned long arg3,
    int *handled_out
)
{
    assert(handled_out);

    enum stdio_node_kind kind;

    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;

    kind = get_stdio_node_kind(node);
    if (kind == STDIO_NODE_NONE) {
        *handled_out = 0;
        return STATUS_NOT_SUPPORTED;
    }

    *handled_out = 1;

    switch (funcid) {
    case BS_FUNCID_SYNC:
        return STATUS_SUCCESS;
    default:
        return STATUS_NOT_SUPPORTED;
    }
}

StStatus StSyscallA_DispatchCallPtr(
    StGnt_Node_StrongRef node,
    uint32_t funcid,
    const void *args,
    void *result,
    unsigned long arg0,
    unsigned long arg1,
    int *handled_out
)
{
    assert(handled_out);

    enum stdio_node_kind kind;
    uint64_t size;

    (void)arg1;

    kind = get_stdio_node_kind(node);
    if (kind == STDIO_NODE_NONE) {
        *handled_out = 0;
        return STATUS_NOT_SUPPORTED;
    }

    *handled_out = 1;

    switch (funcid) {
    case BS_FUNCID_SEEK:
        if (result) *(int64_t *)result = 0;
        return STATUS_NOT_SUPPORTED;
    case BS_FUNCID_TELL:
        if (!result) return STATUS_INVALID_VALUE;
        *(int64_t *)result = 0;
        return STATUS_SUCCESS;
    case BS_FUNCID_READ:
        if (!result) return STATUS_INVALID_VALUE;
        *(uint64_t *)result = 0;
        if (kind != STDIO_NODE_STDIN) return STATUS_END_OF_FILE;
        return STATUS_SUCCESS;
    case BS_FUNCID_WRITE:
        if (!result) return STATUS_INVALID_VALUE;
        size = (uint64_t)arg0;
        *(uint64_t *)result = 0;

        if (kind == STDIO_NODE_STDIN) return STATUS_NOT_SUPPORTED;
        if (size && !args) return STATUS_INVALID_VALUE;

        if (size) {
            stdio_debugcon_write((const uint8_t *)args, size);
        }
        *(uint64_t *)result = size;
        return STATUS_SUCCESS;
    case BS_FUNCID_GETLENGTH:
        if (!result) return STATUS_INVALID_VALUE;
        *(uint64_t *)result = 0;
        return STATUS_NOT_SUPPORTED;
    default:
        return STATUS_NOT_SUPPORTED;
    }
}
