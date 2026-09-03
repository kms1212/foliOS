# 0003: Module Runtime and Interrupt Entry {#decision_0003_module_runtime_and_interrupt_entry}

Status: accepted

Date: 2026-05-17

## Context

foliOS modules are intended to split privileged interrupt handling from the
larger driver body. The kernel-side image must stay small enough to audit and
safe enough to run in interrupt context, while the user-side image owns the
real driver policy, most I/O, and module-facing service code.

The user-side module environment requires a dedicated runtime contract for
module memory, architecture-local protection state, local heaps, interrupt
entry, and driver capabilities. That contract is narrower and more explicit
than a POSIX or C standard library ABI.

## Decision

Represent a module archive as a bundle of separate kernel and user code images
plus declarative module metadata, with each image using a versioned module ABI
rather than a normal program `main` entry.

The optional kernel image is a fast interrupt capsule. Its scope excludes GNT,
SIDL interfaces, service publication, general module policy, and memory
allocation or mapping. It performs only urgent interrupt-context work against
pre-validated module memory, followed by host requests such as EOI, masking, or
waking the user-side module.

The user image is the primary module body and uses a dedicated module runtime.
That runtime may share implementation with the normal user-program libc where
appropriate. Its public contract remains a driver-oriented support library:
bounded memory operations, explicit allocators, module-local state, module
capabilities, and protection-aware entry/return behavior. A separate
compatibility profile may later provide Linux-like driver helpers while native
module APIs remain explicit and statically checkable.

Normal interrupt handling should enter the user module directly when possible.
The user image therefore has a direct interrupt entry in addition to its normal
module entry. The kernel image exists only for cases that need a shorter
early interrupt path before entry into the user-side module.

Use shared interrupt state with a virtual interrupt flag and a pending
indicator. The module runtime may defer interrupt delivery with a local state
update. Resuming should avoid a syscall when no interrupt arrived during the
deferred window. Pending or masked interrupts require a kernel callback for
replay, unmasking, or other host-side cleanup.

Do not expose kernel pointers to the user image. Shared module memory must be
described and validated by the host, and cross-domain data exchange should use
module memory descriptors, offsets, handles, or generated interface bindings
rather than raw cross-domain pointers.

The archive manifest should describe module identity,
ABI requirements, provided interfaces, consumed interfaces, resource needs,
interrupt bindings, memory-region roles, and protection-domain hints. Interface
dependency information is especially important because the kernel may use it as
input when choosing or reconfiguring module protection shards. These records are
planning hints and constraints. The kernel retains authority and must validate
resources, capabilities, ABI versions, and runtime registrations before granting
access.

Provided interface records should embed the compiled SIF artifact and point at
one or more manifest revision extent records. Revision extents use
`[revision_base, revision_base + revision_count)`, so one interface can describe
disjoint supported revision sets without duplicating the interface identity.
Consumed interface records should store only the SIF dependency identity:
interface UUID, root revision hash, required ABI revision, and required revision
hash. Manifest matching uses the SIF dependency identity; interface names remain
diagnostic data from the SIF artifact.

The archive must also carry module-domain authorization. Package signatures
authorize distribution and installation, while module archive signatures
authorize code to execute within module or kernel-adjacent domains. The
module signature must cover the manifest, executable payload digests, ABI
requirements, requested authorities, interrupt-entry claims, and protection
domain constraints that the kernel will use when deciding whether to load the
module.

## Consequences

The module archive format should separate executable payloads from declarative
metadata. The executable payloads remain ELF images, while the manifest gives
the loader and module protection planner enough information to make placement
and protection decisions without parsing module code.

Module archive signatures and package signatures express distinct authorities
and are both required in the normal module loading path. A package may be
allowed to distribute files without being allowed to introduce code into a
module memory domain, direct user IRQ entry, or a kernel-side interrupt capsule.

The manifest should avoid hard-coding physical protection shard numbers. It
should describe dependencies, locality preferences, sharing requirements, and
separation constraints. The kernel remains responsible for translating that
information into the current shard layout, and may change that layout when
modules are loaded, unloaded, or rebound.

The kernel-side module ABI remains intentionally narrow. This reduces the
amount of code allowed in interrupt context and prevents the fast path from
growing into a second in-kernel driver framework.

The user-side module runtime becomes a first-class component that defines module
memory, protection transitions, direct IRQ upcalls, deferred interrupt replay,
and driver-safe utility APIs. Libc compatibility remains a separate concern.

The pending interrupt state is part of the ABI from the beginning because
driver code is expected to defer and resume direct interrupt entry frequently.
Avoiding unconditional resume syscalls is important for keeping direct user IRQ
entry useful on common fast paths.

Linux driver porting should use a compatibility profile layered on top of the
native module runtime while preserving the native runtime contract.

## Related Docs

- [Ambikernel](../architecture/ambikernel.md)
- [Vellum Modules](../vellum/modules.md)
- [Syscall ABI](../sdk/syscall-abi.md)
- [Module Protection Shards and Fault Containment](0006-module-protection-shards-and-fault-containment.md)
