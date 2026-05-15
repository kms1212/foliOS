#ifndef __STRATA_UUID_H__
#define __STRATA_UUID_H__

#include <string.h>

#include <strata/compiler.h>
#include <strata/status.h>
#include <strata/types.h>

struct StUuid {
    uint8_t data[16];
};

#define UUID(...) ((struct StUuid){.data = {__VA_ARGS__}})
#define UUID_INIT(...) {.data = {__VA_ARGS__}}
#define UUID_NULL UUID(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
#define UUID_MAX                                                                                   \
    UUID(                                                                                          \
        0xFF,                                                                                      \
        0xFF,                                                                                      \
        0xFF,                                                                                      \
        0xFF,                                                                                      \
        0xFF,                                                                                      \
        0xFF,                                                                                      \
        0xFF,                                                                                      \
        0xFF,                                                                                      \
        0xFF,                                                                                      \
        0xFF,                                                                                      \
        0xFF,                                                                                      \
        0xFF,                                                                                      \
        0xFF,                                                                                      \
        0xFF,                                                                                      \
        0xFF,                                                                                      \
        0xFF                                                                                       \
    )

static __always_inline int StUuid_IsEqual(
    const struct StUuid *uuid1 __in, const struct StUuid *uuid2 __in
)
{
    return memcmp(uuid1, uuid2, sizeof(struct StUuid)) == 0;
}

static __always_inline int StUuid_IsNil(const struct StUuid *uuid __in)
{
    return StUuid_IsEqual(uuid, &UUID_NULL);
}

static __always_inline int StUuid_IsMax(const struct StUuid *uuid __in)
{
    return StUuid_IsEqual(uuid, &UUID_MAX);
}

static __always_inline int StUuid_GetVersion(const struct StUuid *uuid __in)
{
    return (uuid->data[6] >> 4) & 0x0F;
}

StStatus StUuid_GenerateVersion1(struct StUuid *uuid __buf);
StStatus StUuid_GenerateVersion2(struct StUuid *uuid __buf);
StStatus StUuid_GenerateVersion3(
    struct StUuid *uuid __buf, const char *name __in, size_t name_len __in
);
StStatus StUuid_GenerateVersion4(struct StUuid *uuid __buf);
StStatus StUuid_GenerateVersion5(
    struct StUuid *uuid __buf,
    const struct StUuid *namespace __in,
    const char *name __in,
    size_t name_len __in
);
StStatus StUuid_GenerateVersion6(struct StUuid *uuid __buf);
StStatus StUuid_GenerateVersion7(struct StUuid *uuid __buf);

StStatus StUuid_Validate(const struct StUuid *uuid __in);

#endif  // __STRATA_UUID_H__
