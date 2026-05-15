#ifndef __STRATA_GNT_H__
#define __STRATA_GNT_H__

#include <stddef.h>

#include <strata/compiler.h>
#include <strata/gnt_refs.h>
#include <strata/limits.h>
#include <strata/ref_control.h>
#include <strata/status.h>
#include <strata/utf.h>

/** GNT node kind. */
enum StGnt_NodeType {
    GNT_NODETYPE_LEAF = 0,
    GNT_NODETYPE_DIRECTORY,
    GNT_NODETYPE_LINK,
};

/** Directory iteration record written into caller buffers. */
struct StGnt_DirectoryEntry {
    /** Continuation cookie for the entry. */
    uint64_t cookie;
    /** Total aligned byte length of this record, including name. */
    uint32_t entry_len;
    /** UTF-32 name length in code points. */
    uint16_t name_len;
    /** enum StGnt_NodeType value for the entry. */
    uint16_t type;
    /** Inline UTF-32 name payload. */
    St_Utf32Char name[];
};

struct StModule;
struct StGnt_NodeInterface;

/**
 * Ref-counted Global Node Tree node.
 *
 * Nodes may have both interfaces and children. Handler modules can provide
 * dynamic path resolution and iteration for a subtree without changing the
 * core GNT traversal contract.
 */
struct StGnt_Node {
    /** First-field ref control block used by StGnt_AcquireNode/ReleaseNode. */
    struct StRefControlBlock ref_control;

    /** Global/internal node list link. */
    StGnt_Node_InternalRef next;

    /** Non-owning tree parent and sibling links. */
    StGnt_Node_InternalRef parent;
    StGnt_Node_InternalRef sibling;

    /** UTF-32 node name stored inline. */
    size_t name_len;
    St_Utf32Char name[NODENAME_MAX];

    enum StGnt_NodeType type;

    /*
     * Unified node payload for both file-like and directory-like nodes.
     * A node can have stream/file interfaces and still own children.
     */
    StGnt_Node_InternalRef children_head;
    StGnt_Node_InternalRef children_tail;
    /** Optional module that resolves/iterates dynamic children. */
    struct StModule *handler_module;

    /** Link payload; valid when type is GNT_NODETYPE_LINK. */
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

    /** Node-owner private payload. */
    void *private_data;
};

/**
 * Module-provided path resolver.
 *
 * The resolver receives a base node and the remaining UTF-32 inner path. It
 * returns the next node and a pointer to the unconsumed path suffix.
 */
typedef StStatus (*StGnt_ResolveFunc)(
    StGnt_Node_StrongRef base_node __in,
    const St_Utf32Char *inner_path __in,
    StGnt_Node_StrongRef *next_node __out,
    const St_Utf32Char **remaining_path __out
);

/**
 * Module-provided directory iterator.
 *
 * Writes StGnt_DirectoryEntry records into buffer and returns a continuation
 * cookie. A NULL buffer with nonzero size is invalid; a too-small buffer should
 * return STATUS_BUFFER_TOO_SMALL before partially writing the first entry.
 */
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

/** Initialize the global node tree roots and core system nodes. */
StStatus StGnt_Init(void);
/** Add a child node under parent and optionally return a strong reference. */
StStatus StGnt_AddNode(
    StGnt_Node_StrongRef parent __in,
    const St_Utf32Char *name __in,
    StGnt_Node_StrongRef *node __out_optional
);
/** Remove a node from the tree and release tree-owned references. */
void StGnt_RemoveNode(StGnt_Node_StrongRef node __in);
/** Acquire another strong node reference. */
void StGnt_AcquireNode(StGnt_Node_StrongRef node __inout);
/** Release a strong node reference. */
void StGnt_ReleaseNode(StGnt_Node_StrongRef node __inout);
/** Finalizer used by the node ref-control block. */
void StGnt_FinalizeNode(void *node __in);
/** Resolve a link node to its non-link target. */
StStatus StGnt_ResolveLink(
    StGnt_Node_StrongRef link_node __in, StGnt_Node_StrongRef *target_node __out
);
/** Resolve a UTF-32 path relative to base_node or an absolute GNT root. */
StStatus StGnt_ResolvePath(
    StGnt_Node_StrongRef base_node __in,
    const St_Utf32Char *path __in,
    StGnt_Node_StrongRef *node __out
);
/** Iterate children and dynamic module entries under parent. */
StStatus StGnt_Iterate(
    StGnt_Node_StrongRef parent __in,
    uint64_t cookie __in,
    void *buffer __in,
    size_t buffer_size __in,
    size_t *entry_count __out,
    uint64_t *next_cookie __out
);

#endif  // __STRATA_GNT_H__
