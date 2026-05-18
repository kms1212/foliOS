# 0006: Module Protection Shards and Fault Containment {#decision_0006_module_protection_shards_and_fault_containment}

Status: accepted

Date: 2026-05-18

## Context

foliOS modules are trusted to be non-malicious, signed system components, but
they are not trusted to be crash-free. The module isolation model is therefore
an operational fault-containment mechanism, not a complete sandbox for hostile
code. Its job is to keep ordinary driver and service bugs from corrupting the
kernel, unrelated modules, or unrelated module state while preserving direct
call and direct data-access fast paths where the module ABI permits them.

Some architectures provide fast in-address-space memory protection primitives,
but those primitives are architecture-specific. On amd64 the expected backend
uses MPK/PKRU plus PCID-backed page-table views. Other architectures may use a
different primitive, or may initially provide a slower page-table-only backend.
The kernel should therefore model module isolation in terms of generic module
protection shards rather than exposing MPK as the top-level abstraction.

Existing architectures suggest that the common abstraction should be described
by properties, not by the instruction names of one backend. x86_64 MPK tags
pages with a key and uses thread-local PKRU state to disable read/write data
access. arm64 FEAT_S1POE exposes a similar pkey model through POR_EL0, but its
overlay permissions can cover read, write, and execute. IBM storage protection
keys are also explicitly useful for catching accidental references inside
trusted code. IA-32 segmentation is an older but still useful reference point:
it protects and relocates address ranges through segment descriptors, selectors,
bases, limits, privilege checks, and access attributes rather than through page
tags. ARM MTE, pointer authentication, and RISC-V PMP-like facilities are useful
adjacent mechanisms, but they do not map one-to-one onto the same fast
per-thread protection-window model.

## Decision

Treat a module protection shard as a page-table view with an architecture-local
protection binding table. On amd64, that binding table is implemented with
shard-local pkey assignments. A pkey number is not a global module identity.
The same pkey number may refer to different modules in different shards because
each shard owns a different page-table view and PCID context.

A module protection backend should declare at least these properties:

- binding granularity: page, range, tag granule, or whole address-space view;
- binding mechanism: page-table tag, segment selector/descriptor, range rule,
  address tag, or page-table view;
- binding capacity per shard, including the reserved default binding;
- access dimensions that can be controlled: read, write, execute, or data-only;
- whether the primitive changes address translation, such as a segment base or
  limit, or only overlays permissions on already translated addresses;
- whether protection state is per-thread or per-CPU and whether it is saved in
  the preemption context;
- whether ordinary user code can directly change the protection state;
- transition cost class: register update, page-table switch, TLB invalidation,
  or trap into the kernel;
- whether same-shard transitions are possible without changing the page-table
  view;
- fault reporting quality: whether the handler can identify a protection
  violation, binding id, access type, and faulting address;
- whether the primitive applies to instruction fetch;
- whether the primitive applies only to CPU accesses, with DMA requiring a
  separate IOMMU/device isolation policy.

The generic module runtime may rely only on the common properties it requested
from the active backend. Execute protection, user-writable protection state,
and exact fault metadata are backend capabilities, not universal guarantees.
The initial portable contract should require data read/write containment,
preemption-context protection state, a reserved default binding, explicit KRT
entry/return transitions, and protection-fault attribution to the current module
context.

The module area is organized as large virtual slots. A slot is a stable module
address-space placement unit; the current design target is 64 GiB per slot. A
module's user image and module-owned memory may consume one or more slots. When
a module needs more slot space, the kernel may reserve another slot and bind it
into the same module context.

Within a shard, the default protection binding is reserved for KRT and user-area
access that must remain available to the runtime. On amd64 this is pkey 0.
Module-accessible regions use non-default shard-local bindings. A module's
effective protection identity is therefore the current module context plus the
current shard binding, not an architecture-local key value.

The kernel owns shard planning. It uses module archive metadata, especially SIF
provided/required interface records and other locality or separation hints, to
choose and reconfigure shard layouts. The manifest must not hard-code physical
shard numbers or architecture-local protection keys. Runtime metadata should
carry a shard generation so KRT and fault paths can detect stale call gates or
stale mapping decisions after a reconfiguration.

Shard planning is an incremental optimization pass, not a correctness-critical
part of each call. A valid module graph must keep running under a conservative
layout. The planner may later improve locality, split hot or cold dependencies,
or rebalance slots, but it must do so through generation changes, draining, and
metadata updates rather than assuming module code stores or owns call-frame
state.

KRT is the defined transition coordinator. A same-shard module call may switch
between caller and callee by changing the architecture-local protection state.
On amd64 this means updating PKRU, opening the callee's pkey and closing the
caller according to the module call ABI. A cross-shard module call must also
switch to the target shard's page-table view and architecture context before
entering the callee. On amd64 this includes the target PCID context. On return,
KRT restores the caller's shard view and protection state.

The kernel does not interpret architecture-local module protection permissions
while executing kernel code. Protection state belongs to the preemption context
that will resume module execution. On amd64 this includes the PKRU value. When
an interrupt, fault, or scheduler event enters the kernel, the kernel may save
the interrupted context's protection state and then run with kernel mappings and
kernel rules. If the kernel later enters another module, KRT installs that
module entry's required shard and protection state rather than inheriting
whatever access window was open in the interrupted module.

Ordinary user contexts treat architecture-local protection state as saved user
CPU state rather than a module isolation boundary. User/kernel separation is
provided by the user page-table view and, once implemented, KPTI-style kernel
mapping removal on kernel entry. On amd64, the default ordinary-user PKRU value
should leave all pkeys open. The kernel may save and restore that value across
preemption like other user context state without assigning module-runtime
meaning to it.

Well-defined cross-module calls must not depend on the caller's private stack
remaining accessible while the caller's private protection binding is closed.
KRT should define the entry frame, return frame, and stack-switching rules
explicitly. Callees should run on a callee, KRT, or otherwise ABI-defined stack,
not by accidentally continuing on caller-owned private memory.

Modules must not store KRT call frames as module-owned state. Call frames are
runtime-owned transition records whose lifetime is tied to the active call
chain and preemption context. Module-local state may store ordinary driver or
service state, but it must not be required to reconstruct the KRT return path
after a fault, interrupt, shard transition, or planner reconfiguration.

Cross-module data access should be mediated by module-runtime descriptors,
offsets, handles, or generated interface bindings rather than ad hoc raw
pointers. The runtime may temporarily open the required architecture-local
binding while performing a validated access, then close it again.

A jump into another module's code without going through the KRT call gate is
undefined module behavior, not a valid cross-module call. MPK does not prevent
instruction fetch from another mapped executable page on amd64, and foliOS does
not require the generic protection-shard abstraction to enforce execute
isolation. If a bad function pointer lands in another module's code, that code
does not receive a valid entry frame, module context, or open data binding set.
The expected outcome is a module-context fault, failed ABI check, or invalid
runtime access, after which the kernel and KRT attribute the failure to the
current module context and recover by tearing down, detaching, or restarting
that context rather than panicking the whole system.

Module loading may still perform static contract validation, such as requiring
ELF `ET_DYN` images, enforcing W^X mappings, and rejecting ABI-forbidden
instructions outside approved runtime code. These checks enforce the module ABI
and reduce accidental escape from the fault-containment model; they are not a
claim that modules are hostile-code sandboxes.

## Consequences

Architecture-local protection-key limits are local to a shard instead of global
to the system. On amd64, foliOS can host more module protection domains by
adding page-table views and reusing pkey numbers across those views.

Shard transitions are more expensive than same-shard protection-state changes,
so the kernel's shard planner becomes performance-sensitive. Modules that
frequently call each other or share descriptors should usually be placed in the
same shard when resource and separation constraints allow it.

Fault handling must carry enough execution metadata to identify the current
module context, current shard, architecture-local binding, slot, and shard
generation. Module faults should normally become module-context termination,
restart, or capability revocation events. Kernel faults and module-loader
invariant failures remain stronger errors.

KRT call frames need a clear nesting and unwind model. Nested module calls must
restore shard and protection state on normal return and on fault paths. Invalid
or stale call frames should fail as module runtime faults.

Slot management needs explicit lifecycle rules: reservation, sparse mapping,
growth to additional slots, deallocation, and guard placement. Slot size should
make module identity checks cheap while leaving enough virtual space for sparse
placement and future ASLR or layout randomization.

Page-table and architecture-context management becomes part of the module
runtime contract. On amd64 this includes PCID. Shard reconfiguration must update
or invalidate the affected page-table views, TLB entries, and KRT metadata
without leaving old bindings silently usable. Because the planner is
incremental, module execution must be correct before and after optimization,
with generation checks protecting transitions across layout changes.

Direct interrupt entry must be integrated with the current shard and module
context. VIF/pending-interrupt state, late masking, and deferred replay need to
survive module faults and shard transitions without losing interrupt ownership
or unmasking the wrong line.

Debugging and observability should report module id, slot, shard generation,
and architecture-local binding for module faults. Without that metadata, fault
containment will be hard to distinguish from ordinary kernel memory faults
during bring-up.

## Related Docs

- [Module Runtime and Interrupt Entry](0003-module-runtime-and-interrupt-entry.md)
- [Package-Managed Execution](0004-package-managed-execution.md)
- [Ambikernel](../architecture/ambikernel.md)
