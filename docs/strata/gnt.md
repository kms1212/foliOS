# Global Node Tree {#strata_gnt}

This page describes the Global Node Tree model, node ownership, path
resolution, handles, and node-backed process resources.

## Model

The Global Node Tree is a hierarchical namespace for kernel-visible resources.
Nodes may represent directories, leaves, or links. A directory can also have a
handler module, which lets static kernel children and delegated module-provided
entries coexist under one path.

Important root nodes include:

- `g_gnt_root_local`: `/`;
- `g_gnt_root_network`: `//`.

## Path Resolution

`StGnt_ResolvePath` handles:

- absolute local paths beginning with `/`;
- network-root paths beginning with `//`;
- relative paths from a base node;
- `.` and `..`;
- link resolution with loop-depth protection;
- delegated module resolution when a node has a resolver.

The resolver contract returns the next node and the remaining path. That keeps
dynamic namespace semantics localized to the module that owns them.

## Iteration

`StGnt_Iterate` writes `struct StGnt_DirectoryEntry` records into a caller
buffer and returns a continuation cookie. Static child cookies and module
cookies are separate so iteration can move from kernel-owned children into a
module-provided listing without losing position.

## Interfaces And Handles

Nodes can register interfaces identified by UUID and ABI version. Querying a
node returns a function-id base and negotiated ABI version. Handles retain node
objects for userspace-facing access; handle-table close/clear paths release the
underlying object according to handle type.

Use `StUuid_IsEqual` for UUID comparison. Do not add local UUID equality helpers.
