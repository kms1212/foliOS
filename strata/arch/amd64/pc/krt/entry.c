#include <strata/compiler.h>
#include <strata/status.h>
#include <strata/uuid.h>

struct entries {
    StStatus (*node_open)(const uint8_t *path, uint32_t flags, uint32_t *handle);
    StStatus (*node_close)(uint32_t handle);
    StStatus (*node_get_interface_funcid_base)(
        uint32_t handle,
        const struct StUuid *if_uuid,
        uint32_t request_abiver,
        uint32_t *funcid_base,
        uint32_t *result_abiver
    );
    StStatus (*node_call)(uint32_t handle, uint32_t funcid, ...);
};

static StStatus node_open(const uint8_t *path, uint32_t flags, uint32_t *handle)
{
    return STATUS_NOT_IMPLEMENTED;
}

static StStatus node_close(uint32_t handle)
{
    return STATUS_NOT_IMPLEMENTED;
}

static StStatus node_get_interface_funcid_base(
    uint32_t handle,
    const struct StUuid *if_uuid,
    uint32_t request_abiver,
    uint32_t *funcid_base,
    uint32_t *result_abiver
)
{
    return STATUS_NOT_IMPLEMENTED;
}

static StStatus node_call(uint32_t handle, uint32_t funcid, ...)
{
    return STATUS_NOT_IMPLEMENTED;
}

__section(".krt_table") const struct entries g_entries = {
    .node_open = node_open,
    .node_close = node_close,
    .node_get_interface_funcid_base = NULL,
    .node_call = NULL,
};
