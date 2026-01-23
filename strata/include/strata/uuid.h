#ifndef __STRATA_UUID_H__
#define __STRATA_UUID_H__

#include <string.h>

#include <strata/types.h>
#include <strata/compiler.h>
#include <strata/status.h>

struct StUuid {
    uint8_t data[16];
};

#define UUID(...) ((struct StUuid){ .data = { __VA_ARGS__ } })
#define UUID_NULL UUID(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
#define UUID_MAX  UUID(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF)

static __always_inline int StUuid_IsEqual(struct StUuid *uuid1, struct StUuid *uuid2)
{
    return memcmp(uuid1, uuid2, sizeof(struct StUuid)) == 0;
}

static __always_inline int StUuid_IsNil(struct StUuid *uuid)
{
    return StUuid_IsEqual(uuid, &UUID_NULL);
}

static __always_inline int StUuid_IsMax(struct StUuid *uuid)
{
    return StUuid_IsEqual(uuid, &UUID_MAX);
}

static __always_inline int StUuid_GetVersion(struct StUuid *uuid)
{
    return (uuid->data[6] >> 4) & 0x0F;
}

StStatus StUuid_GenerateVersion1(struct StUuid *uuid);
StStatus StUuid_GenerateVersion2(struct StUuid *uuid);
StStatus StUuid_GenerateVersion3(struct StUuid *uuid, const char *name, size_t name_len);
StStatus StUuid_GenerateVersion4(struct StUuid *uuid);
StStatus StUuid_GenerateVersion5(struct StUuid *uuid, const struct StUuid *namespace, const char *name, size_t name_len);
StStatus StUuid_GenerateVersion6(struct StUuid *uuid);
StStatus StUuid_GenerateVersion7(struct StUuid *uuid);

StStatus StUuid_Validate(struct StUuid *uuid);

#endif // __STRATA_UUID_H__
