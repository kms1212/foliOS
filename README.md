# foliOS

foliOS is an experimental operating system built around **Strata**, a kernel
that explores the **Ambikernel** model. Conventional process isolation remains
the baseline. Selected signed modules may execute in explicit kernel-adjacent
domains when lower transition overhead justifies the additional runtime
contract.

This repository contains experimental research code. Its documentation records
implemented behavior, intended invariants, and planned design work; some
mechanisms remain incomplete.

## Core Ideas

Strata separates execution into three conceptual strata:

- User space contains ordinary processes with isolated address spaces. They
  access kernel services through KRT syscall entry points.
- Signed modules may execute in kernel-adjacent domains under explicit runtime
  gates, module contexts, and protection state.
- The kernel retains authority over physical memory, page tables, scheduling,
  interrupt routing, handle tables, and system-wide recovery.

The Ambikernel model keeps component ownership visible across separate domains.
Selected driver and service paths may avoid process-style IPC when the ABI
clearly specifies memory lifetime, authority, and fault attribution.

The design relies on the following mechanisms:

- Module protection shards provide scalable module isolation without making
  architecture-specific hardware-key limits part of the generic ABI.
- KRT defines the entry points for current user-to-kernel transitions and
  planned module calls.
- Cross-domain sharing uses descriptors with explicit ownership, lifetime, and
  revocation rules rather than raw pointers.
- GNT provides a capability-oriented global namespace for kernel services,
  device interfaces, files, and resources managed by the planned package
  system.
- `stload` defines the normalized boot-information contract between the loader
  and Strata.

## Current Focus

- Strata's memory model and module isolation boundary.
- The `stload` loader-to-kernel handoff ABI.
- Vellum's BIOS boot path and QEMU-based development workflow.
- The foliSDK build pipeline for the kernel, runtime, and user-space
  components.

## Components

- Strata is the kernel and the project's main architectural experiment.
- Vellum provides the BIOS bootloader core and its module environment.
- `load_folios` is a Vellum module that loads Strata and builds the `stload`
  handoff table passed to the kernel.
- foliSDK supplies the toolchain, C runtime, and build environment for the
  kernel and user-space components.
- `docs/` contains architecture notes, subsystem references, ADRs, and
  development guides.

## Read Next

- [Architecture](docs/architecture/index.md)
- [Ambikernel](docs/architecture/ambikernel.md)
- [Boot Flow](docs/architecture/boot-flow.md)
- [Strata Memory](docs/strata/memory/index.md)
- [Global Node Tree](docs/strata/gnt.md)
- [`stload` ABI](docs/common/stload-abi.md)
- [Build](docs/development/build.md)
- [Disk Images](docs/development/disk-images.md)

## Build & Run

Prerequisites:

- Python 3 and the
  [foliSDK host prerequisites](folisdk/README.md#prerequisites).
- `i686-elf-gcc` for the IA-32 BIOS bootloader build.
- QEMU for local testing.

```sh
git clone https://github.com/kms1212/foliOS --recursive
cd foliOS

(cd folisdk && ./build.py --arch x86_64 --builddir-layout --jobs 18)
export PATH="$(pwd)/folisdk/build/folisdk-host/bin:$(pwd)/folisdk/build/folisdk-x86_64/bin:$PATH"

folisdk/build/folisdk-host/bin/cmake \
    -S . \
    -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DTARGET=amd64-pc-bios \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

folisdk/build/folisdk-host/bin/cmake --build build --parallel=18

scripts/mkdisk.sh -a ia32 disk.img
scripts/run.sh --disk disk.img pc-amd64
```

The `-a ia32` disk-image option selects the BIOS bootloader architecture. For
the `amd64-pc-bios` target, Strata targets AMD64, but the Vellum BIOS loader is
built for IA-32.

## License

foliOS, Vellum, and Strata are licensed under the Apache License, Version 2.0.
See [LICENSE](LICENSE) for details.
