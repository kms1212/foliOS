# Build {#development_build}

The root CMake project drives host tools directly and builds Vellum and Strata
as external projects with component-specific toolchains.

## Configure

The common development target is BIOS-based AMD64:

```sh
cmake -S . -B build -DTARGET=amd64-pc-bios
```

`TARGET` follows the `arch-platform-firmware` shape. The top-level build derives
`TARGET_ARCH`, `TARGET_PLATFORM`, and `TARGET_FIRMWARE` from that value, then
passes the relevant pieces into the Vellum and Strata external projects.

For AMD64 Strata, Vellum is still built as IA-32:

```cmake
if ("${TARGET_ARCH}" STREQUAL "amd64")
    set(VELLUM_TARGET_ARCH ia32)
endif()
```

That split is also reflected in disk image creation.

## Build

Build everything:

```sh
cmake --build build
```

Build one side explicitly:

```sh
cmake --build build --target vellum
cmake --build build --target strata
```

The external project build directories are normally:

- `build/vellum`;
- `build/strata`;
- `build/docs/doxygen` for generated documentation.

## Documentation

Generate all documentation outputs:

```sh
cmake --build build --target docs-doxygen-all
```

The generated references are split by component:

- `build/docs/doxygen/html`: top-level conceptual docs;
- `build/docs/doxygen/strata/html`: Strata reference;
- `build/docs/doxygen/vellum/html`: Vellum reference;
- `build/docs/doxygen/common/html`: common loader ABI reference.

## Static Analysis

Configure with clang-tidy enabled:

```sh
cmake -S . -B build -DTARGET=amd64-pc-bios -DENABLE_CLANG_TIDY=ON
```

Run the scripted targets after the component build exists:

```sh
cmake --build build --target tidy-strata
cmake --build build --target tidy-vellum
cmake --build build --target tidy-all
```

The tidy scripts use the root `.clang-tidy` configuration and add target-aware
compiler arguments so kernel and bootloader headers are parsed under the right
architecture.
