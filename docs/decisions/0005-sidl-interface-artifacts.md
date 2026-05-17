# 0005: SIDL Interface Artifacts {#decision_0005_sidl_interface_artifacts}

Status: accepted

Date: 2026-05-17

## Context

SIDL source files are edited by humans and may contain formatting, comments,
and generation hints that are not themselves ABI authority. Treating the source
text and an `abirevision` number as the only compatibility record makes it too
easy for ABI history to fragment or for an old revision to be rewritten without
a clear artifact-level signal.

Modules and packages also need to depend on interface identity in a way that is
stable without requiring the old `.sidl` source files to be present forever.

## Decision

Keep `.sidl` as a source format and introduce a compiled SIDL interface
artifact, `.sif`, as the format used for distribution, dependency resolution,
code generation, and ABI history extension.

The `.sif` artifact is a Strata InterFace binary tree representation of the
parsed interface, not a container for the original `.sidl` source text. It uses
the `SIF\0` magic value and stores interface metadata, mandatory interface UUID
identity, mandatory generated-code prefix metadata, ABI revisions,
declarations, parameter/type trees, and revision hash links in a form that
tools can validate and consume directly. Strings in the artifact are padded so
the next field starts on a 4-byte boundary.

The interface UUID identity is artifact metadata, not a normal annotation. The
namespace UUID is stored as 16 binary bytes and the name is stored as a string
in the artifact header. The generated-code prefix is also stored in the header
instead of the generic annotation list.

`sidlc compile` takes a `.sidl` source file and, when extending an existing
interface, the previous `.sif` artifact. It emits a new `.sif` that contains the
previous ABI revision chain plus the newly compiled revisions from source.
Previous revisions are taken from the `.sif` artifact, not reconstructed from
old source files.

Each ABI revision is chained to the previous revision by hash. The revision hash
domain uses the `strata.` prefix, includes the UUIDv5 result derived from the
interface UUID namespace and name, then includes the binary tree for that ABI
revision. A tool that reads the artifact must verify that the revision numbers
are contiguous and that each revision hash matches the stored tree and
previous-revision hash.

The first revision hash is also the lineage hash for the interface. Interface
dependencies must not trust namespace/name alone. They should pin the
UUIDv5-derived interface UUID, the root revision hash, the required revision
number, and the required revision hash. The UUID names the interface, the root
revision hash verifies that the provider belongs to the expected ABI lineage,
and the required revision hash verifies the exact ABI surface the consumer was
built against.

`sidlc generate` takes a `.sif` artifact as input and emits language bindings.
Generated headers and sources must not depend on reparsing `.sidl` source or on
recovering embedded source text from the artifact.

`sidlc decompile` converts a `.sif` artifact back into canonical `.sidl` source
for inspection, review, or source recovery. The decompiled output is a source
view of the artifact, not the authority for already published revisions.

## SIF File Format

All multi-byte integers are little-endian. The format version remains `0` until
the format is externally distributed.

Strings use a 4-byte length followed by raw bytes and zero padding so the next
field starts on a 4-byte boundary:

```text
sif_string {
    byte_length u32
    bytes[byte_length]
    zero_padding[(4 - byte_length % 4) % 4]
}
```

The file header and revision table are:

```text
sif_file {
    magic[4]              // "SIF\0"
    format_version u32    // currently 0

    interface_name sif_string
    uuid_namespace[16]
    uuid_name sif_string
    prefix sif_string
    interface_annotations annotations

    revision_count u32
    root_revision_hash[32]
    revision_hash_table[revision_count][32]

    revision_trees[revision_count]
}
```

`root_revision_hash` must equal `revision_hash_table[0]`. Tools can use the
revision table for dependency matching without first walking every declaration
tree. They must still validate the tree hashes before trusting the artifact.

The common node encodings are:

```text
annotations {
    count u32
    annotation[count]
}

annotation {
    name sif_string
    arg_count u32
    expression[arg_count]
}

expression {
    kind u8               // 1 string, 2 number, 3 identifier
    payload               // sif_string, u64, or sif_string
}

type {
    is_const u8
    kind u8               // 1 named, 2 ptr, 3 array
    payload               // named type string or nested type
}
```

Each revision tree is stored as:

```text
revision_tree {
    version u64
    previous_revision_hash[32]
    annotations

    bitfield_count u32
    bitfield[bitfield_count]

    enum_count u32
    enum[enum_count]

    struct_count u32
    struct[struct_count]

    function_count u32
    function[function_count]
}
```

Declaration nodes are:

```text
bitfield {
    name sif_string
    annotations
    base_type type
    field_count u32
    fields[field_count] { name sif_string, bits u64 }
}

enum {
    name sif_string
    annotations
    base_type type
    member_count u32
    members[member_count] { name sif_string, value u64, annotations }
}

struct {
    name sif_string
    annotations
    field_count u32
    fields[field_count] { type, name sif_string }
}

function {
    id u32
    name sif_string
    annotations
    parameter_count u32
    parameter[parameter_count]
}

parameter {
    direction u8          // 0 in, 1 out, 2 inout
    annotations
    type
    name sif_string
}
```

Revision hashes are computed as:

```text
revision_hash = SHA256(
    sif_string("strata.sif.revision.v0"),
    uuidv5(uuid_namespace, uuid_name)[16],
    revision_tree_binary
)
```

For revision `0`, `previous_revision_hash` is all zeroes. For later revisions,
it must equal the previous entry in `revision_hash_table`.

## Dependency Matching

Package and module dependency records should include:

```text
sidl_dependency {
    interface_uuid[16]        // uuidv5(uuid_namespace, uuid_name)
    root_revision_hash[32]
    required_revision u64
    required_revision_hash[32]
}
```

Matching a provider `.sif` against a dependency proceeds as:

1. Compare `interface_uuid`.
2. Compare `root_revision_hash`.
3. Check `required_revision < revision_count`.
4. Compare `revision_hash_table[required_revision]` with
   `required_revision_hash`.
5. Validate the provider's revision trees before treating the artifact as
   trusted input.

## Consequences

The interface artifact becomes an append-only ABI log. A new revision can be
created from the previous `.sif` plus new source, so old source files are not
required in order to preserve ABI history.

Interface dependencies in packages and module manifests can refer to `.sif`
identity rather than raw source text. This gives package and module signing
policy a stable object to approve.

The `.sif` format can later add authority signatures and dependency metadata
without changing the authoring role of `.sidl`.

Code generation becomes a pure projection from the compiled interface artifact
to a target language and architecture. That separation makes generated code
less sensitive to source formatting and parser evolution.

## Related Docs

- [Package-Managed Execution](0004-package-managed-execution.md)
- [SDK and Runtime](../sdk/index.md)
- [Syscall ABI](../sdk/syscall-abi.md)
