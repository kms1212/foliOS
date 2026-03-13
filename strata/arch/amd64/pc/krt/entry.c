#include <strata/compiler.h>
#include <strata/status.h>
#include <strata/uuid.h>

struct entries {
    StStatus (*node_open)(const uint8_t *path __in, uint32_t flags __in, uint32_t *handle __out);
    StStatus (*node_close)(uint32_t handle __in);
    StStatus (*node_query)(
        uint32_t handle __in,
        const struct StUuid *if_uuid __in,
        uint32_t request_groupid __in,
        uint32_t request_abiver __in,
        uint32_t *funcid_base __out,
        uint32_t *result_abiver __out
    );
    StStatus (*node_call0)(uint32_t handle __in, uint32_t funcid __in);
    StStatus (*node_call1)(uint32_t handle __in, uint32_t funcid __in, unsigned long arg0 __in);
    StStatus (*node_call2)(
        uint32_t handle __in, uint32_t funcid __in, unsigned long arg0 __in, unsigned long arg1 __in
    );
    StStatus (*node_call3)(
        uint32_t handle __in,
        uint32_t funcid __in,
        unsigned long arg0 __in,
        unsigned long arg1 __in,
        unsigned long arg2 __in
    );
    StStatus (*node_call4)(
        uint32_t handle __in,
        uint32_t funcid __in,
        unsigned long arg0 __in,
        unsigned long arg1 __in,
        unsigned long arg2 __in,
        unsigned long arg3 __in
    );
    StStatus (*node_call_n)(
        uint32_t handle __in,
        uint32_t funcid __in,
        const void *args __in,
        void *result __buf,
        unsigned long arg0 __in,
        unsigned long arg1 __in
    );
};

StStatus node_open_syscall(const uint8_t *path __in, uint32_t flags __in, uint32_t *handle __out);
StStatus node_close_syscall(uint32_t handle __in);
StStatus node_query_syscall(
    uint32_t handle __in,
    const struct StUuid *if_uuid __in,
    uint32_t request_groupid __in,
    uint32_t request_abiver __in,
    uint32_t *funcid_base __out,
    uint32_t *result_abiver __out
);
StStatus node_call0_syscall(uint32_t handle __in, uint32_t funcid __in);
StStatus node_call1_syscall(uint32_t handle __in, uint32_t funcid __in, unsigned long arg0 __in);
StStatus node_call2_syscall(
    uint32_t handle __in, uint32_t funcid __in, unsigned long arg0 __in, unsigned long arg1 __in
);
StStatus node_call3_syscall(
    uint32_t handle __in,
    uint32_t funcid __in,
    unsigned long arg0 __in,
    unsigned long arg1 __in,
    unsigned long arg2 __in
);
StStatus node_call4_syscall(
    uint32_t handle __in,
    uint32_t funcid __in,
    unsigned long arg0 __in,
    unsigned long arg1 __in,
    unsigned long arg2 __in,
    unsigned long arg3 __in
);
StStatus node_call_n_syscall(
    uint32_t handle __in,
    uint32_t funcid __in,
    const void *args __in,
    void *result __buf,
    unsigned long arg0 __in,
    unsigned long arg1 __in
);

__section(".rodata.krt_table") const struct entries g_entries = {
    .node_open = node_open_syscall,
    .node_close = node_close_syscall,
    .node_query = node_query_syscall,
    .node_call0 = node_call0_syscall,
    .node_call1 = node_call1_syscall,
    .node_call2 = node_call2_syscall,
    .node_call3 = node_call3_syscall,
    .node_call4 = node_call4_syscall,
    .node_call_n = node_call_n_syscall,
};
