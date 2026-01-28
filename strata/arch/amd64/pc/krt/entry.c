#include <strata/compiler.h>
#include <strata/status.h>
#include <strata/syscall.h>
#include <strata/uuid.h>

extern StStatus do_syscall(
    uint64_t rdi __in,
    uint64_t rsi __in,
    uint64_t rdx __in,
    uint64_t r10 __in,
    uint64_t r8 __in,
    uint64_t rax __in
);

extern StStatus do_node_call_syscall(
    uint64_t rdi __in,
    uint64_t rsi __in,
    uint64_t rdx __in,
    uint64_t r10 __in,
    uint64_t r8 __in,
    uint64_t r9 __in
);

struct entries {
    StStatus (*node_open)(const uint8_t *path __in, uint32_t flags __in, uint32_t *handle __out);
    StStatus (*node_close)(uint32_t handle __in);
    StStatus (*node_get_interface_funcid_base)(
        uint32_t handle __in,
        const struct StUuid *if_uuid __in,
        uint32_t request_abiver __in,
        uint32_t *funcid_base __out,
        uint32_t *result_abiver __out
    );
    StStatus (*node_call)(
        uint32_t handle __in,
        uint32_t funcid __in,
        const void *args __in,
        size_t args_size __in,
        void *result __buf,
        size_t result_size __in
    );
};

static StStatus node_open(const uint8_t *path __in, uint32_t flags __in, uint32_t *handle __out)
{
    return do_syscall((uint64_t)path, flags, (uint64_t)handle, 0, 0, SYS_NODE_OPEN);
}

static StStatus node_close(uint32_t handle __in)
{
    return do_syscall((uint64_t)handle, 0, 0, 0, 0, SYS_NODE_CLOSE);
}

static StStatus node_get_interface_funcid_base(
    uint32_t handle __in,
    const struct StUuid *if_uuid __in,
    uint32_t request_abiver __in,
    uint32_t *funcid_base __out,
    uint32_t *result_abiver __out
)
{
    return do_syscall(
        (uint64_t)handle,
        (uint64_t)if_uuid,
        request_abiver,
        (uint64_t)funcid_base,
        (uint64_t)result_abiver,
        SYS_NODE_GET_INTERFACE_FUNCID_BASE
    );
}

static StStatus node_call(
    uint32_t handle __in,
    uint32_t funcid __in,
    const void *args __in,
    size_t args_size __in,
    void *result __buf,
    size_t result_size __in
)
{
    // this call is special, since its argument count is 6, so we need to
    // hardcode the syscall number directly in assembly
    return do_node_call_syscall(
        (uint64_t)handle,
        funcid,
        (uint64_t)args,
        args_size,
        (uint64_t)result,
        result_size
    );
}

__section(".rodata.krt_table") const struct entries g_entries = {
    .node_open = node_open,
    .node_close = node_close,
    .node_get_interface_funcid_base = node_get_interface_funcid_base,
    .node_call = node_call,
};
