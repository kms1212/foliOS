# Debugging {#development_debugging}

This page describes QEMU, panic logs, stack traces, symbol lookup, and useful
debug commands.

## Running Under QEMU

After building and creating a disk image, use:

```sh
scripts/run.sh --disk disk.img pc-amd64
```

The exact QEMU invocation is kept in the script so target-specific options can
evolve without duplicating them in documentation.

Use `scripts/monitor.sh` when you want QEMU, debug output, optional GDB, and
optional CPU/memory plots in one tmux session:

```sh
scripts/monitor.sh --disk disk.img pc-amd64
```

Common monitor options:

```sh
scripts/monitor.sh --disk build/disk.img --session folios pc-amd64
scripts/monitor.sh --no-plots --disk disk.img pc-amd64
scripts/monitor.sh --no-gdb --disk disk.img pc-amd64
```

Arguments after the machine name are passed through to QEMU via `run.sh`.

## Panic Output

Strata's AMD64 PC panic path prints the panic status and can walk frame pointers
for the current kernel stack or early boot stack. Symbol formatting uses the
static Strata symbol table path so panic-time lookup does not depend on disk,
heap-heavy ELF parsing, or other services that may be compromised.

If stack traces are empty or stop too early, check:

- whether the code was built with frame pointers preserved;
- whether the fault happened on a known kernel or early stack;
- whether the static symbol information was linked into the kernel image.

## Symbols

`StSymbol_LookupStatic` and `StSymbol_FormatStatic` use the static symbol table.
The generic `StSymbol_Lookup` and `StSymbol_Format` entry points are reserved
for richer runtime resolution, but the panic path should stay on the static
variant unless it is proven safe in interrupt and panic contexts.

## GDB

Use the project script as the starting point:

```sh
scripts/gdb.sh
```

Keep GDB-only debugging separate from kernel logging. Kernel logs are part of
the system's diagnostic contract; debugger state is a local development aid.
