# clang-tidy {#development_clang_tidy}

This page describes standard clang-tidy usage and foliOS custom checks.

## Running

Configure with clang-tidy enabled:

```sh
cmake -S . -B build -DTARGET=amd64-pc-bios -DENABLE_CLANG_TIDY=ON
```

Then run the component target:

```sh
cmake --build build --target tidy-strata
cmake --build build --target tidy-vellum
```

The scripts add the target triple used by the corresponding external project.
For AMD64 Strata, Vellum still receives an i686-style target because the BIOS
bootloader side is IA-32.

## foliOS Plugin

Build the plugin and run only the foliOS checks:

```sh
cmake --build build --target FoliosClangTidyPlugin
scripts/clang_tidy.sh --build-dir build --component strata --folios
```

The CMake convenience targets do the same thing when the plugin target exists:

```sh
cmake --build build --target tidy-folios-strata
cmake --build build --target tidy-folios-vellum
cmake --build build --target tidy-folios-all
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
