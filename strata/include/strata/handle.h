#ifndef __STRATA_HANDLE_H__
#define __STRATA_HANDLE_H__

#include <stdint.h>

#include <strata/status.h>

typedef uint32_t StHandle_Id __nocast;
typedef StHandle_Id StHandle __nocast;

enum StHandle_Type {
    ST_HANDLE_TYPE_NONE = 0,
    ST_HANDLE_TYPE_GNT_NODE,
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

void StHandle_InitTable(struct StHandle_Table *table __inout);
StStatus StHandle_Create(
    struct StHandle_Table *table __inout,
    enum StHandle_Type type __in,
    void *object __in,
    StHandle_Id *handle_out __out
);
StStatus StHandle_Get(
    struct StHandle_Table *table __in,
    StHandle_Id handle __in,
    enum StHandle_Type *type_out __out_optional,
    void **object_out __out_optional
);
StStatus StHandle_GetRetained(
    struct StHandle_Table *table __in,
    StHandle_Id handle __in,
    enum StHandle_Type *type_out __out_optional,
    void **object_out __out_optional
);
void StHandle_ReleaseObject(enum StHandle_Type type __in, void *object __in);
StStatus StHandle_Close(struct StHandle_Table *table __inout, StHandle_Id handle __in);
void StHandle_ClearTable(struct StHandle_Table *table __inout);

#endif  // __STRATA_HANDLE_H__
