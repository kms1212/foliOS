# Ambikernel {#architecture_ambikernel}

The Ambikernel model used by Strata retains conventional process isolation and
adds an explicit module stratum for selected kernel-adjacent drivers and
services. It permits justified low-overhead paths while keeping authority,
memory ownership, and fault attribution visible to the kernel.

## Execution Strata

Strata separates execution into three conceptual strata:

- User space runs ordinary process code in per-process address spaces and uses
  handles and KRT syscall entry points to access kernel services.
- Signed modules may execute in kernel-adjacent domains under an explicit module
  context, protection state, and runtime entry rules.
- The kernel retains authority over physical memory, page tables, scheduling,
  process and thread lifetime, handle tables, interrupt routing, and system-wide
  recovery.

The module stratum contains signed, non-malicious code that may still crash. The
kernel should attribute a module bug to its module context and recover by
terminating, restarting, or detaching that context, or by revoking its
capabilities. The intended recovery path contains the failure before it corrupts
arbitrary kernel state.

## Memory Layout

The baseline memory model remains process-oriented: user mappings are local to
an address space, while kernel and runtime mappings are controlled by Strata.
The Ambikernel extension adds a global module region with large virtual slots.
A module image and module-owned memory are placed into one or more slots, and
slot ownership becomes part of the module context.

Module memory occupies a distinct region outside ordinary user mappings and the
kernel heap. Cross-module data sharing should use module-runtime descriptors,
offsets, handles, or generated interface bindings. Raw pointers hide lifetime,
access, and fault-attribution rules and therefore fall outside the baseline
contract.

## Protection Shards

Module protection shards implement module isolation. A shard combines a
page-table view with an architecture-local protection binding table. This keeps
the generic kernel model independent of any single backend, such as AMD64
MPK/PKRU.

Within a shard, same-shard calls may switch only architecture-local
protection state. Cross-shard calls switch to the target shard's page-table view
and architecture context before entering the callee. The kernel owns shard
planning and may use module metadata to group hot dependencies, split unrelated
components, or reconfigure layouts with explicit generation changes.

A module's effective protection identity consists of its module context, current
shard binding, and shard generation. Hardware protection-key identifiers remain
local to a shard.

## Runtime Transitions

KRT coordinates user-to-kernel and module runtime entry. A valid module call
must enter through a defined runtime gate so the callee receives the expected
module context, stack contract, protection binding, and return frame.

Module calls must stop using the caller's private stack once the caller's
private protection binding is closed. The runtime contract must
define where call frames live, how nested calls unwind, and how protection state
is restored on normal return, fault, interrupt, or scheduler entry.

Only a KRT gate establishes a valid cross-module call. A direct jump into
another module's code lacks a module context and runtime frame and should fail
as a module runtime fault.

## Kernel Authority

The kernel retains authority over operations that can corrupt global system
state:

- physical frame ownership;
- page-table and address-space mutations;
- module slot reservation and mapping;
- shard planning and generation changes;
- interrupt ownership and replay;
- process/thread lifetime and scheduling;
- handle tables, GNT authority, and capability-bearing objects.

Fast paths may avoid process-style IPC, while ownership-changing work still
follows these rules. Such work transfers control to kernel-owned code or a
KRT-defined transition point.

## Fault Containment

A module fault should carry enough metadata to identify the module context,
slot, shard, architecture-local binding, and shard generation involved. That
metadata lets the kernel distinguish an ordinary module containment event from
a kernel invariant failure.

Fault containment protects the system from failures in trusted modules.
Hostile-code sandboxing lies outside its scope. Module signatures, ELF
validation, W^X mapping, ABI checks, and runtime gates reduce accidental
violations of the isolation contract. The kernel still treats modules as trusted
system components with constrained failure domains.

## Related Docs

- [Memory Model](memory-model.md)
- [Module Runtime and Interrupt Entry](../decisions/0003-module-runtime-and-interrupt-entry.md)
- [Module Protection Shards and Fault Containment](../decisions/0006-module-protection-shards-and-fault-containment.md)
