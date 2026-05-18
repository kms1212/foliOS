# 0003: Module Runtime and Interrupt Entry {#decision_0003_module_runtime_and_interrupt_entry}

Status: accepted

Date: 2026-05-17

## Context

foliOS modules are intended to split privileged interrupt reaction from the
larger driver body. The kernel-side image must stay small enough to audit and
safe enough to run in interrupt context, while the user-side image owns the
real driver policy, most I/O, and module-facing service code.

The module user environment also cannot be treated as a normal process libc
environment. Module memory, architecture-local protection state, local heaps,
interrupt entry, and driver capabilities need a runtime contract that is
narrower and more explicit than a POSIX or C standard library ABI.

## Decision

Represent a module archive as a bundle of separate kernel and user code images
plus declarative module metadata, with each image using a versioned module ABI
rather than a normal program `main` entry.

The kernel image is an optional fast interrupt capsule. It must not know about
GNT, SIDL interfaces, service publication, or general module policy. It must not
allocate, map, unmap, or free memory. Its normal job is limited to urgent
interrupt-context work against pre-validated module memory, followed by a
request for host actions such as EOI, masking, or waking the user side.

The user image is the primary module body. It uses a dedicated module runtime,
not the normal user-program libc. That runtime may reuse implementation pieces
where safe, but its public contract is a driver-oriented support library:
bounded memory operations, explicit allocators, module-local state, module
capabilities, and protection-aware entry/return behavior. A separate
compatibility profile may later provide Linux-like driver helpers, but native
module APIs should remain explicit and statically checkable.

Normal interrupt handling should enter the user module directly when possible.
The user image therefore has a direct interrupt entry in addition to its normal
module entry. The kernel image exists only for cases that need a shorter
pre-user reaction path.

Use a shared interrupt state with a virtual interrupt flag and a pending
indicator. Defer may be handled as a local state update by the module runtime.
Resume should avoid a syscall when no interrupt arrived during the deferred
window, but must call back into the kernel when pending or masked interrupt
state needs replay, unmasking, or other host-side cleanup.

Do not expose kernel pointers to the user image. Shared module memory must be
described and validated by the host, and cross-domain data exchange should use
module memory descriptors, offsets, handles, or generated interface bindings
rather than raw cross-domain pointers.

The archive manifest should be expressive enough to describe module identity,
ABI requirements, provided interfaces, consumed interfaces, resource needs,
interrupt bindings, memory-region roles, and protection-domain hints. Interface
dependency information is especially important because the kernel may use it as
input when choosing or reconfiguring module protection shards. These records are
planning hints and constraints, not ambient authority: the kernel must still
validate resources, capabilities, ABI versions, and runtime registrations before
granting access.

Provided interface records should embed the compiled SIF artifact and point at
one or more manifest revision extent records. Revision extents use
`[revision_base, revision_base + revision_count)`, so one interface can describe
disjoint supported revision sets without duplicating the interface identity.
Consumed interface records should store only the SIF dependency identity:
interface UUID, root revision hash, required ABI revision, and required revision
hash. Interface names are diagnostic data from the SIF artifact, not manifest
matching keys.

The archive must also carry module-domain authorization. Package signatures
authorize distribution and installation, while module archive signatures
authorize code to attach to module or kernel-adjacent execution domains. The
module signature must cover the manifest, executable payload digests, ABI
requirements, requested authorities, interrupt-entry claims, and protection
domain constraints that the kernel will use when deciding whether to load the
module.

## Consequences

The module archive format should separate executable payloads from declarative
metadata. The executable payloads remain ELF images, while the manifest gives
the loader and module protection planner enough information to make placement
and protection decisions without parsing module code.

Module archive signatures are a required part of the normal module loading
path. They are not a replacement for package signatures; they express a
different authority. A package may be allowed to distribute files without being
allowed to introduce code into a module memory domain, direct user IRQ entry,
or a kernel-side interrupt capsule.

The manifest should avoid hard-coding physical protection shard numbers. It
should describe dependencies, locality preferences, sharing requirements, and
separation constraints. The kernel remains responsible for translating that
information into the current shard layout, and may change that layout when
modules are loaded, unloaded, or rebound.

The kernel-side module ABI remains intentionally narrow. This reduces the
amount of code allowed in interrupt context and prevents the fast path from
growing into a second in-kernel driver framework.

The user-side module runtime becomes a first-class component. It is not a libc
porting layer; it is the place where module memory, protection transitions,
direct IRQ upcalls, deferred interrupt replay, and driver-safe utility APIs are
defined.

The pending interrupt state is part of the ABI from the beginning because
driver code is expected to defer and resume direct interrupt entry frequently.
Avoiding unconditional resume syscalls is important for keeping direct user IRQ
entry useful on common fast paths.

Linux driver porting should be handled as a compatibility profile layered on
top of the native module runtime, not by weakening the native runtime contract.

## Related Docs

- [Ambikernel](../architecture/ambikernel.md)
- [Vellum Modules](../vellum/modules.md)
- [Syscall ABI](../sdk/syscall-abi.md)
- [Module Protection Shards and Fault Containment](0006-module-protection-shards-and-fault-containment.md)
