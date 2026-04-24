#ifndef __STRATA_GNT_INTERFACE_H__
#define __STRATA_GNT_INTERFACE_H__

#include <stdint.h>

#include <strata/gnt.h>
#include <strata/status.h>
#include <strata/uuid.h>

struct StGnt_NodeInterface {
    struct StGnt_NodeInterface *next;

    struct StUuid uuid;
    uint32_t abi_version;
    uint32_t funcid_span;
};

extern const struct StUuid StGntIf_Uuid_ByteStream;
extern const struct StUuid StGntIf_Uuid_Directory;
extern const struct StUuid StGntIf_Uuid_FileInfo;
extern const struct StUuid StGntIf_Uuid_Process;
extern const struct StUuid StGntIf_Uuid_Thread;

StStatus StGnt_RegisterInterface(
    struct StGnt_Node *node __inout,
    const struct StUuid *if_uuid __in,
    uint32_t abi_version __in,
    uint32_t funcid_span __in
);
StStatus StGnt_QueryInterface(
    struct StGnt_Node *node __in,
    const struct StUuid *if_uuid __in,
    uint32_t request_abiver __in,
    uint32_t *funcid_base_out __out_optional,
    uint32_t *result_abiver_out __out_optional
);

#endif  // __STRATA_GNT_INTERFACE_H__
