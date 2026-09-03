# Process {#strata_process}

This page describes Strata process objects, address spaces, handles, and exit
state.

Keep ABI-facing process behavior in the SDK documentation unless it is a kernel
contract.

## Object Model

`struct StProcess` is a ref-counted object. It contains state, owned objects,
and non-owning links:

- process id, type, state, and exit status;
- a GNT node for namespace-visible process resources;
- platform process data;
- one `StAddressSpace_StrongRef`;
- a main thread and intrusive thread list;
- a handle table;
- module/TLS metadata;
- one `StAllocationOwner_StrongRef`.

The allocation owner is separate from the address space. The address space owns
the local virtual domain; the allocation owner owns memory charge and cleanup.

## Creation

`StProcess_CreateUser` and `StProcess_CreateModule` return a required
`StProcess_StrongRef * __out`. Creation initializes the owner, address space,
handle table, process state, and list/GNT visibility before the process becomes
observable.

New process code should not create a process and then discard the strong output
unless ownership is explicitly transferred by the API.

## Removal

Process removal is phased:

- `StProcess_BeginRemove` marks the process dying and detaches it from public
  lookup.
- `StProcess_FinalizeRemove` releases subresources such as handles, address
  space, allocation owner, and main-thread ownership.
- `StProcess_Remove` coordinates the two when immediate removal is possible.

This split lets thread exit, wait/detach behavior, and deferred reap cooperate
without racing a raw pointer that may already have been freed.

## Exit Status

The kernel stores process exit state as `StStatus`. Runtime-specific conversion
from C/POSIX-style `main` return values belongs in the runtime path, not in the
generic kernel process object. Kernel code should treat exit status as a typed
Strata status value.
