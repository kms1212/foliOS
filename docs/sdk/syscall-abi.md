# Syscall ABI {#sdk_syscall_abi}

This page describes syscall numbering, calling convention, status returns, and
SDK wrapper contracts.

## Kernel Surface

The kernel syscall surface currently covers node/handle-oriented operations:

- open a GNT path;
- close a handle;
- query a node interface by UUID and ABI version;
- call a function using register arguments;
- call a function with pointer/result arguments.

The kernel returns `StStatus`. Successful calls may also write output
registers or output buffers according to the specific operation.

## SDK Handles

`libstrata` represents a user-space handle as an opaque `StHandle`. Internally,
it stores the kernel handle number and a small interface-query cache.
`StHandle_Open` allocates a local handle object after the kernel open succeeds;
`StHandle_Close` closes the kernel handle and releases the local object.

`StHandle_Query` negotiates an interface UUID and ABI version. Successful query
results provide a function-id base, so generated bindings can call the correct
node function without hard-coding global syscall numbers.

## KRT Entry Table

The runtime initializes `libstrata` with the KRT/sysinfo entry table before
opening standard process handles. SDK wrapper calls go through that table
instead of embedding architecture assembly at every call site.

On AMD64, the low-level kernel entry path follows the platform syscall
convention, but user code should normally call SDK wrappers or generated SIDL
bindings rather than issuing syscalls directly.

## Status Discipline

Every SDK wrapper returning `StStatus` should follow the same rule as kernel
code: check, return, or explicitly ignore the status. Compatibility functions
that must expose POSIX `errno`-style results should perform the conversion at
the wrapper boundary.
