# Ambikernel {#architecture_ambikernel}

The Ambikernel model used by Strata keeps conventional process isolation as the
baseline, then adds an explicit module stratum for selected kernel-adjacent
drivers and services. The goal is not to collapse every component into one
trusted address space. The goal is to let low-overhead paths exist where they
are justified while keeping authority, memory ownership, and fault attribution
visible to the kernel.

## Execution Strata

Strata separates execution into three conceptual strata:

- **User**: ordinary process code with per-process address spaces, handles, and
  syscall/KRT entry into kernel services.
- **Module**: signed system components that may run closer to the kernel fast
  path than an ordinary process, but with explicit module context, protection
  state, and runtime entry rules.
- **Kernel**: the authority for physical memory, page tables, scheduling,
  process/thread lifetime, handle tables, interrupt routing, and global recovery.

The module stratum is trusted to be non-malicious but not trusted to be
crash-free. A module bug should normally be attributable to a module context and
recoverable as a module fault, restart, detach, or capability-revocation event
rather than an arbitrary kernel corruption.

## Memory Shape

The baseline memory model remains process-oriented: user mappings are local to
an address space, while kernel and runtime mappings are controlled by Strata.
The Ambikernel extension adds a global module region with large virtual slots.
A module image and module-owned memory are placed into one or more slots, and
slot ownership becomes part of the module context.

Module memory is not ordinary user memory and not ordinary kernel heap memory.
Cross-module data sharing should use module-runtime descriptors, offsets,
handles, or generated interface bindings. Raw pointer sharing across module
boundaries is not the baseline contract because it hides lifetime, access, and
fault-attribution rules.

## Protection Shards

Module isolation is expressed through module protection shards. A shard is a
page-table view plus an architecture-local protection binding table. This keeps
the generic kernel model independent from a single backend such as amd64
MPK/PKRU.

Within a shard, same-shard calls may be able to switch only architecture-local
protection state. Cross-shard calls switch to the target shard's page-table view
and architecture context before entering the callee. The kernel owns shard
planning and may use module metadata to group hot dependencies, split unrelated
components, or reconfigure layouts through generation changes.

Architecture-local key numbers are not module identities. A module's effective
protection identity is the module context plus the current shard binding and
shard generation.

## Runtime Transitions

KRT is the transition coordinator for user/kernel and module-facing runtime
entry. A valid module call must enter through a defined runtime gate so the
callee receives the expected module context, stack rule, protection binding,
and return frame.

Module calls must not accidentally continue on caller-private stack memory once
the caller's private protection binding is closed. The runtime contract must
define where call frames live, how nested calls unwind, and how protection state
is restored on normal return, fault, interrupt, or scheduler entry.

Direct jumps into another module's code are not valid cross-module calls. They
do not establish a module context or runtime frame and should fail as module
runtime faults rather than becoming an implicit authority transfer.

## Kernel Authority

The kernel remains the authority for operations that can corrupt global system
state:

- physical frame ownership;
- page-table and address-space mutations;
- module slot reservation and mapping;
- shard planning and generation changes;
- interrupt ownership and replay;
- process/thread lifetime and scheduling;
- handle tables, GNT authority, and capability-bearing objects.

Fast paths may avoid process-style IPC, but they do not bypass these ownership
rules. If a fast path needs authority-changing work, the operation returns to
kernel-owned code or a KRT-defined transition point.

## Fault Containment

A module fault should carry enough metadata to identify the module context,
slot, shard, architecture-local binding, and shard generation involved. That
metadata lets the kernel distinguish an ordinary module containment event from
a kernel invariant failure.

Fault containment is operational, not a hostile-code sandbox. Module signatures,
ELF validation, W^X mapping, ABI checks, and runtime gates reduce accidental
escape from the model, but the kernel still treats modules as trusted system
components with constrained failure domains.

## Related Docs

- [Memory Model](memory-model.md)
- [Module Runtime and Interrupt Entry](../decisions/0003-module-runtime-and-interrupt-entry.md)
- [Module Protection Shards and Fault Containment](../decisions/0006-module-protection-shards-and-fault-containment.md)
