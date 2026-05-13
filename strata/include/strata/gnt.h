#ifndef __STRATA_GNT_H__
#define __STRATA_GNT_H__

#include <stddef.h>

#include <strata/compiler.h>
#include <strata/limits.h>
#include <strata/ref_control.h>
#include <strata/status.h>
#include <strata/utf.h>

#ifndef __STRATA_GNT_NODE_REFS_DEFINED__
#    define __STRATA_GNT_NODE_REFS_DEFINED__
struct StGnt_Node;
typedef struct StGnt_Node *StGnt_Node_StrongRef __ref_strong;
typedef struct StGnt_Node *StGnt_Node_WeakRef __ref_weak;
typedef struct StGnt_Node *StGnt_Node_BorrowedRef __ref_borrowed;
typedef struct StGnt_Node *StGnt_Node_InternalRef __ref_internal;
#endif

enum StGnt_NodeType {
    GNT_NODETYPE_LEAF = 0,
    GNT_NODETYPE_DIRECTORY,
    GNT_NODETYPE_LINK,
};

struct StGnt_DirectoryEntry {
    uint64_t cookie;
    uint32_t entry_len;
    uint16_t name_len;
    uint16_t type;
    St_Utf32Char name[];
};

struct StModule;
struct StGnt_NodeInterface;

struct StGnt_Node {
    struct StRefControlBlock ref_control;

    StGnt_Node_InternalRef next;

    StGnt_Node_InternalRef parent;
    StGnt_Node_InternalRef sibling;

    size_t name_len;
    St_Utf32Char name[NODENAME_MAX];

    enum StGnt_NodeType type;

    /*
     * Unified node payload for both file-like and directory-like nodes.
     * A node can have stream/file interfaces and still own children.
     */
    StGnt_Node_InternalRef children_head;
    StGnt_Node_InternalRef children_tail;
    struct StModule *handler_module;

    struct {
        int is_virtual;

        struct {
            StGnt_Node_InternalRef target_node;
        } virtual;

        struct {
            size_t target_path_len;
            St_Utf32Char *target_path;
        } physical;
    } link;

    struct StGnt_NodeInterface *interface_head;
    struct StGnt_NodeInterface *interface_tail;

    void *private_data;
};

typedef StStatus (*StGnt_ResolveFunc)(
    StGnt_Node_StrongRef base_node __in,
    const St_Utf32Char *inner_path __in,
    StGnt_Node_StrongRef *next_node __out,
    const St_Utf32Char **remaining_path __out
);

typedef StStatus (*StGnt_IterateFunc)(
    StGnt_Node_StrongRef parent __in,
    uint64_t cookie __in,
    void *buffer __in,
    size_t buffer_size __in,
    size_t *entry_count __out,
    uint64_t *next_cookie __out
);

extern StGnt_Node_StrongRef g_gnt_root_network;  // "//"
extern StGnt_Node_StrongRef g_gnt_root_local;    // "/"
extern StGnt_Node_StrongRef g_gnt_system_processes;

StStatus StGnt_Init(void);
StStatus StGnt_AddNode(
    StGnt_Node_StrongRef parent __in,
    const St_Utf32Char *name __in,
    StGnt_Node_StrongRef *node __out_optional
);
void StGnt_RemoveNode(StGnt_Node_StrongRef node __in);
void StGnt_AcquireNode(StGnt_Node_StrongRef node __inout);
void StGnt_ReleaseNode(StGnt_Node_StrongRef node __inout);
void StGnt_FinalizeNode(void *node __in);
StStatus StGnt_ResolveLink(
    StGnt_Node_StrongRef link_node __in, StGnt_Node_StrongRef *target_node __out
);
StStatus StGnt_ResolvePath(
    StGnt_Node_StrongRef base_node __in,
    const St_Utf32Char *path __in,
    StGnt_Node_StrongRef *node __out
);
StStatus StGnt_Iterate(
    StGnt_Node_StrongRef parent __in,
    uint64_t cookie __in,
    void *buffer __in,
    size_t buffer_size __in,
    size_t *entry_count __out,
    uint64_t *next_cookie __out
);

#endif  // __STRATA_GNT_H__
