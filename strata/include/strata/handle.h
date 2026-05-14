#ifndef __STRATA_HANDLE_H__
#define __STRATA_HANDLE_H__

#include <stdint.h>

#include <strata/status.h>
#include <strata/uuid.h>

typedef uint32_t StHandle_Id __nocast;
typedef StHandle_Id StHandle __nocast;

enum StHandle_Type {
    HANDLE_TYPE_NONE = 0,
    HANDLE_TYPE_GNT_NODE,
};

struct StHandle_Entry {
    struct StHandle_Entry *next;

    StHandle_Id id;
    enum StHandle_Type type;
    void *object;
};

struct StHandle_Table {
    struct StHandle_Entry *head;
    struct StHandle_Entry *tail;
    StHandle_Id next_id;
};

void StHandle_TableInit(struct StHandle_Table *table __inout);
StStatus StHandle_TableCreate(
    struct StHandle_Table *table __inout,
    enum StHandle_Type type __in,
    void *object __in,
    StHandle_Id *handle_out __out
);
StStatus StHandle_TableGet(
    struct StHandle_Table *table __in,
    StHandle_Id handle __in,
    enum StHandle_Type *type_out __out_optional,
    void **object_out __out_optional
);
StStatus StHandle_TableGetRetained(
    struct StHandle_Table *table __in,
    StHandle_Id handle __in,
    enum StHandle_Type *type_out __out_optional,
    void **object_out __out_optional
);
void StHandle_TableReleaseObject(enum StHandle_Type type __in, void *object __in);
StStatus StHandle_TableClose(struct StHandle_Table *table __inout, StHandle_Id handle __in);
void StHandle_TableClear(struct StHandle_Table *table __inout);

StStatus StHandle_Open(const uint8_t *path __in, uint32_t flags __in, StHandle *handle __out);
StStatus StHandle_Close(StHandle handle __in);
StStatus StHandle_Query(
    StHandle handle __in,
    const struct StUuid *if_uuid __in,
    uint32_t request_abiver __in,
    uint32_t *funcid_base __out,
    uint32_t *result_abiver __out
);
StStatus StHandle_Call0(StHandle handle __in, uint32_t funcid __in);
StStatus StHandle_Call1(StHandle handle __in, uint32_t funcid __in, unsigned long arg0 __in);
StStatus StHandle_Call2(
    StHandle handle __in, uint32_t funcid __in, unsigned long arg0 __in, unsigned long arg1 __in
);
StStatus StHandle_Call3(
    StHandle handle __in,
    uint32_t funcid __in,
    unsigned long arg0 __in,
    unsigned long arg1 __in,
    unsigned long arg2 __in
);
StStatus StHandle_Call4(
    StHandle handle __in,
    uint32_t funcid __in,
    unsigned long arg0 __in,
    unsigned long arg1 __in,
    unsigned long arg2 __in,
    unsigned long arg3 __in
);
StStatus StHandle_CallN(
    StHandle handle __in,
    uint32_t funcid __in,
    const void *args __in,
    void *result __out_optional,
    unsigned long arg0 __in,
    unsigned long arg1 __in
);
StStatus StHandle_CallReg(
    StHandle handle __in,
    uint32_t funcid __in,
    unsigned long arg0 __in,
    unsigned long arg1 __in,
    unsigned long arg2 __in,
    unsigned long arg3 __in
);
StStatus StHandle_CallPtr(
    StHandle handle __in,
    uint32_t funcid __in,
    const void *args __in,
    void *result __out_optional,
    unsigned long arg0 __in,
    unsigned long arg1 __in
);

#endif  // __STRATA_HANDLE_H__
