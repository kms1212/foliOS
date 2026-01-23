#include <strata/uuid.h>

StStatus StUuid_GenerateVersion1(struct StUuid *uuid);
StStatus StUuid_GenerateVersion2(struct StUuid *uuid);
StStatus StUuid_GenerateVersion3(struct StUuid *uuid, const char *name, size_t name_len);
StStatus StUuid_GenerateVersion4(struct StUuid *uuid);
StStatus StUuid_GenerateVersion5(struct StUuid *uuid, const char *name, size_t name_len);
StStatus StUuid_GenerateVersion6(struct StUuid *uuid);
