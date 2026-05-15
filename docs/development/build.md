# Build {#development_build}

The root CMake project drives host tools, user-space programs, Vellum, and
Strata from the foliSDK-provided CMake. Build foliSDK first and keep its host
and target tool directories at the front of `PATH` while configuring foliOS.

## SDK Toolchain

Build foliSDK with the build-directory layout:

```sh
(cd folisdk && ./build.py --arch x86_64 --builddir-layout --jobs 18)
```

Then expose the SDK tools from the root of the foliOS checkout:

```sh
export PATH="$(pwd)/folisdk/build/folisdk-host/bin:$(pwd)/folisdk/build/folisdk-x86_64/bin:$PATH"
```

Use `folisdk/build/folisdk-host/bin/cmake` for configure and build commands.
This keeps the kernel, bootloader, SDK runtime, and user-space programs on the
same toolchain path.

## Configure

The common development target is BIOS-based AMD64:

```sh
folisdk/build/folisdk-host/bin/cmake \
    -S . \
    -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DTARGET=amd64-pc-bios \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
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
folisdk/build/folisdk-host/bin/cmake --build build --parallel=18
```

Build one side explicitly:

```sh
folisdk/build/folisdk-host/bin/cmake --build build --target vellum
folisdk/build/folisdk-host/bin/cmake --build build --target strata
```

The external project build directories are normally:

- `build/vellum`;
- `build/strata`;
- `build/docs/doxygen` for generated documentation.

## Documentation

Generate all documentation outputs:

```sh
folisdk/build/folisdk-host/bin/cmake --build build --target docs-doxygen-all
```

Doxygen HTML uses `doxygen-awesome-css` by default. CMake downloads the pinned
release into the build directory during configuration instead of tracking it as
a submodule. Disable the theme with `-DFOLIOS_DOXYGEN_AWESOME=OFF`.

The generated references are split by component:

- `build/docs/doxygen/html`: top-level conceptual docs;
- `build/docs/doxygen/strata/html`: Strata reference;
- `build/docs/doxygen/vellum/html`: Vellum reference;
- `build/docs/doxygen/common/html`: common loader ABI reference.

## Static Analysis

Configure with clang-tidy enabled:

```sh
folisdk/build/folisdk-host/bin/cmake \
    -S . \
    -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DTARGET=amd64-pc-bios \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DENABLE_CLANG_TIDY=ON
```

Run the scripted targets after the component build exists:

```sh
folisdk/build/folisdk-host/bin/cmake --build build --target tidy-strata
folisdk/build/folisdk-host/bin/cmake --build build --target tidy-vellum
folisdk/build/folisdk-host/bin/cmake --build build --target tidy-all
```

The tidy scripts use the root `.clang-tidy` configuration and add target-aware
compiler arguments so kernel and bootloader headers are parsed under the right
architecture.
