# clang-tidy {#development_clang_tidy}

This page describes standard clang-tidy usage and foliOS custom checks.

## Running

Configure with clang-tidy enabled:

```sh
export PATH="$(pwd)/folisdk/build/folisdk-host/bin:$(pwd)/folisdk/build/folisdk-x86_64/bin:$PATH"

folisdk/build/folisdk-host/bin/cmake \
    -S . \
    -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DTARGET=amd64-pc-bios \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DENABLE_CLANG_TIDY=ON
```

Then run the component target:

```sh
folisdk/build/folisdk-host/bin/cmake --build build --target tidy-strata
folisdk/build/folisdk-host/bin/cmake --build build --target tidy-vellum
```

The scripts add the target triple used by the corresponding external project.
For AMD64 Strata, Vellum receives an i686-style target because the BIOS
bootloader targets IA-32.

Both the CMake integration and `scripts/clang_tidy.sh` run clang-tidy in quiet
mode so hidden-header statistics such as `Suppressed N warnings` do not obscure
the actionable diagnostics.

## foliOS Plugin

Build the plugin and run only the foliOS checks:

```sh
folisdk/build/folisdk-host/bin/cmake --build build --target FoliosClangTidyPlugin
scripts/clang_tidy.sh --build-dir build --component strata --folios
```

The CMake convenience targets do the same thing when the plugin target exists:

```sh
folisdk/build/folisdk-host/bin/cmake --build build --target tidy-folios-strata
folisdk/build/folisdk-host/bin/cmake --build build --target tidy-folios-vellum
folisdk/build/folisdk-host/bin/cmake --build build --target tidy-folios-all
```

`--folios` is shorthand for loading the plugin from the build directory and
using the `folios-*` check filter. The long form is still available when testing
an out-of-tree plugin:

```sh
scripts/clang_tidy.sh \
    --build-dir build \
    --component strata \
    --folios-plugin build/tools/clang-tidy-folios/libFoliosClangTidyPlugin.so \
    --folios-checks
```

Useful shorthands:

```sh
scripts/clang_tidy.sh --strata --folios
scripts/clang_tidy.sh --vellum
scripts/clang_tidy.sh --all
```

## foliOS Checks

The local plugin lives under `tools/clang-tidy-folios`. Current checks include:

- `folios-api-annotations`: public API parameters need direction annotations.
- `folios-api-nullability`: null handling must match direction annotations.
- `folios-distinct-typedefs`: `__nocast`, `__bitwise`, and reference typedefs
  should have type meaning.
- `folios-nullized-params`: `__nullized` and `__success_nullized` pointer slots
  must be cleared by the callee.
- `folios-status-must-check`: `StStatus` results must be checked, returned, or
  intentionally ignored with `(void)`.

These checks are intended to enforce API contracts, not formatting taste. The
normal CMake `tidy-*` targets are useful for the broad clang-tidy profile; the
manual `--folios` path is useful when iterating on the local plugin checks.

## Triage

Analyzer checks such as dead stores can mix real signal with noise from macro
expansion, error-handling progress flags, or code excluded by configuration.
Prefer fixing the cases where the warning points to unclear ownership or stale
state. If a warning is structurally intentional, use a narrow suppression and
explain the reason.

Use `(void)` when ignoring a status is the actual policy:

```c
(void)StUtf_ConvertUtf8ToUtf32(...);
```

Do not hide ignored statuses in unused temporaries.
