#include <strata/uuid.h>

StStatus StUuid_GenerateVersion1(struct StUuid *uuid __buf)
{
    return STATUS_UNIMPLEMENTED;
}

StStatus StUuid_GenerateVersion2(struct StUuid *uuid __buf)
{
    return STATUS_UNIMPLEMENTED;
}

StStatus StUuid_GenerateVersion3(
    struct StUuid *uuid __buf, const char *name __in, size_t name_len __in
)
{
    return STATUS_UNIMPLEMENTED;
}

StStatus StUuid_GenerateVersion4(struct StUuid *uuid __buf)
{
    return STATUS_UNIMPLEMENTED;
}

StStatus StUuid_GenerateVersion5(
    struct StUuid *uuid __buf,
    const struct StUuid *namespace __in,
    const char *name __in,
    size_t name_len __in
)
{
    return STATUS_UNIMPLEMENTED;
}

StStatus StUuid_GenerateVersion6(struct StUuid *uuid __buf)
{
    return STATUS_UNIMPLEMENTED;
}
