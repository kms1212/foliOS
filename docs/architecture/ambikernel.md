# Ambikernel {#architecture_ambikernel}

The Ambikernel model used by foliOS is a research architecture rather than a
drop-in clone of a monolithic kernel, microkernel, or exokernel. Its main idea
is to keep the system's fast paths close to the kernel while making isolation
domains explicit enough that hardware features such as page tables, PCID, and
MPK can eventually enforce them at lower cost than process-style IPC.

## Current Shape

The current tree is organized around three major layers:

- `vellum`: the bootloader. It discovers enough machine state to load Strata and
  loads the `loadst` bootloader module that builds the boot information table.
- `strata`: the kernel. It owns physical memory, virtual address spaces,
  scheduling, process/thread lifetime, the Global Node Tree, syscall dispatch,
  and early/panic diagnostics.
- `folisdk`: the user/runtime side. It contains the C runtime integration,
  `libstrata`, syscall/KRT-facing wrappers, and compatibility work.

The kernel still presents conventional process address spaces. That is
intentional: the Ambikernel design does not remove the traditional memory model.
It treats it as the compatibility and safety baseline, then adds faster shared
or protected domains where they can be justified.

## Design Direction

The longer term design is to make selected drivers and system services run in
less-privileged module domains while preserving low-overhead data access where
hardware isolation permits it. In the README this is described with MPK, PCID,
KRT, and module-sharding terminology. Those should be read as architecture
goals unless the specific subsystem documentation says the mechanism is already
implemented.

The important contract is not "everything shares one pointer space." The
important contract is that every boundary should be visible in code:

- ownership is visible through typed references and `StRefControlBlock`;
- memory placement is visible through MM/VMM flags and address-space scope;
- public API intent is visible through annotations such as `__in`, `__out`,
  `__buf`, `__ref_strong`, and `__nocast`;
- status propagation is visible through checked `StStatus` results.

## Kernel Boundary

Strata remains the authority for operations that can corrupt global system
state: page-table updates, physical frame ownership, process/thread lifetime,
handle tables, and scheduler state. A module may eventually have a fast path for
some operation, but the kernel contract should still be expressed in a form that
can be audited and checked.

That is why the codebase leans toward explicit types and local policy
conversion. It is better for an API to be slightly noisier and statically
checkable than for a resource boundary to be hidden behind a generic helper.
