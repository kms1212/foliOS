#ifndef __STRATA_GNT_H__
#define __STRATA_GNT_H__

#include <stdatomic.h>
#include <stddef.h>

#include <strata/limits.h>
#include <strata/status.h>
#include <strata/utf.h>

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
    struct StGnt_Node *next;

    struct StGnt_Node *parent;
    struct StGnt_Node *sibling;

    size_t name_len;
    St_Utf32Char name[NODENAME_MAX];

    enum StGnt_NodeType type;

    /*
     * Unified node payload for both file-like and directory-like nodes.
     * A node can have stream/file interfaces and still own children.
     */
    struct StGnt_Node *children_head;
    struct StGnt_Node *children_tail;
    struct StModule *handler_module;

    struct {
        int is_virtual;

        struct {
            struct StGnt_Node *target_node;
        } virtual;

        struct {
            size_t target_path_len;
            St_Utf32Char *target_path;
        } physical;
    } link;

    struct StGnt_NodeInterface *interface_head;
    struct StGnt_NodeInterface *interface_tail;

    atomic_uint ref_count;

    void *private_data;
};

typedef StStatus (*StGnt_ResolveFunc)(
    struct StGnt_Node *base_node __in,
    const St_Utf32Char *inner_path __in,
    struct StGnt_Node **next_node __out,
    const St_Utf32Char **remaining_path __out
);

typedef StStatus (*StGnt_IterateFunc)(
    struct StGnt_Node *parent __in,
    uint64_t cookie __in,
    void *buffer __in,
    size_t buffer_size __in,
    size_t *entry_count __out,
    uint64_t *next_cookie __out
);

extern struct StGnt_Node *g_gnt_root_network;  // "//"
extern struct StGnt_Node *g_gnt_root_local;    // "/"
extern struct StGnt_Node *g_gnt_system_processes;

StStatus StGnt_Init(void);
StStatus StGnt_AddNode(
    struct StGnt_Node *parent __in,
    const St_Utf32Char *name __in,
    struct StGnt_Node **node __out_optional
);
StStatus StGnt_RemoveNode(struct StGnt_Node *node __in);
void StGnt_AcquireNode(struct StGnt_Node *node __inout);
void StGnt_ReleaseNode(struct StGnt_Node *node __inout);
StStatus StGnt_ResolveLink(
    struct StGnt_Node *link_node __in, struct StGnt_Node **target_node __out
);
StStatus StGnt_ResolvePath(
    struct StGnt_Node *base_node __in, const St_Utf32Char *path __in, struct StGnt_Node **node __out
);
StStatus StGnt_Iterate(
    struct StGnt_Node *parent __in,
    uint64_t cookie __in,
    void *buffer __in,
    size_t buffer_size __in,
    size_t *entry_count __out,
    uint64_t *next_cookie __out
);

#endif  // __STRATA_GNT_H__
