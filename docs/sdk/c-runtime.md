# C Runtime {#sdk_c_runtime}

This page describes the foliOS C runtime, startup objects, process bootstrap,
and runtime/kernel boundary.

## Components

The runtime is currently split across:

- musl-derived startup and libc code in `folisdk/musl-strata`;
- Strata-facing handle wrappers in `folisdk/libstrata`;
- generated or hand-written SIDL bindings for kernel interfaces;
- the KRT/sysinfo entry table passed to the process by the kernel.

POSIX semantics remain in the runtime, which translates C and libc conventions
into Strata status and handle operations.

## Process Bootstrap

The libc startup path initializes:

- environment and auxiliary vector state;
- page size and hardware capability fields;
- `AT_SYSINFO` / KRT entry table;
- standard Strata handles for process, main thread, stdin, stdout, stderr;
- file descriptor glue;
- TLS and stack protector state;
- init arrays.

The startup code calls the application entry point after completing those
steps.

## Exit

`exit` runs libc finalizers, stdio cleanup, and then terminates through the
process interface. `_Exit` skips libc finalizers and terminates directly.

Both paths convert an integer exit code into a `StStatus` in
`STATUS_AREA_PROCESS_EXIT` at the runtime boundary. The current conversion uses
the low 8 bits of the C exit code and sets the failure bit for nonzero values.
