# foliOS clang-tidy checks

This directory contains out-of-tree clang-tidy checks for foliOS-specific API
contracts.

## Checks

### `folios-api-annotations`

Warns when a parameter in a public `strata` or `vellum` header lacks an API
annotation such as `__in`, `__out`, `__inout`, `__out_optional`, or `__buf`.

The first version intentionally treats `__buf` as sufficient. This keeps the
initial signal practical while the codebase is still converging on direction
annotations.

### `folios-api-nullability`

Checks the nullability contract implied by direction annotations on function
definitions.

`__out` and `__inout` parameters are non-optional output parameters. They should
be asserted at function entry, and `if`-based NULL checks on them are reported.
Do not mechanically convert such parameters to `__out_optional`: creation,
allocation, open, and ownership-transfer APIs usually require a non-NULL output
slot so the caller can receive and eventually release the resource.

The check allows a narrow thin-wrapper exception: if the whole function body is
`return Callee(...);` and the non-optional output parameter is passed directly to
a callee parameter that is also annotated `__out` or `__inout`, the wrapper may
rely on the callee's entry assertion. The exception is intentionally not applied
to wrappers that perform validation, cleanup, logging, or other local behavior.

`__out_optional` parameters are optional outputs. Asserting them at entry is
reported because callers are allowed to pass NULL.

### `folios-distinct-typedefs`

Gives `__bitwise`, `__nocast`, and reference ownership typedefs a Sparse-like
meaning in clang-tidy. The check reports implicit conversions between distinct
tagged typedef domains. Explicit casts are treated as intentional boundary
crossings.

By default, `__bitwise` is strict even when the other side is a plain integer
type. `__nocast` is domain-to-domain by default, so existing numeric helper
macros do not drown out the higher-signal mistakes. Set `StrictNocast=true` to
also report plain integer conversions. Integer constant expressions are still
allowed as `__nocast` values so common literals and numeric constants remain
usable without casts.

`__unit_count(unit)` and `__unit_index(unit, domain)` describe `__nocast`
numeric domains that can participate in constrained arithmetic. A page index may
be added to or subtracted from a page count, and subtracting two indexes in the
same unit/domain produces a count. Index arithmetic does not cross domains:
`St_VirtPage` and `St_PhysFrame` both use page units, but their virtual and
physical index domains are distinct.

`__flagset(domain)` describes `__nocast` bitmask domains. Bitwise operations
preserve the flagset only when both operands belong to the same flag domain, or
when the other operand is an integer constant expression.

Reference typedefs use the plain annotation names `ref_strong`, `ref_weak`,
`ref_borrowed`, `ref_internal`, and `ref_locked`. They are domain-to-domain by default:
implicit conversion between different ref types, or between different object ref
domains, is reported. Set `StrictRefs=true` to also report conversions between
raw pointers and ref typedefs.

Strata object reference typedefs are declared next to the object that owns the
type, not in a central `refs.h`. When headers need only a forward reference to a
related object, they may repeat the guarded typedef block for that object. The
guard name must be shared by all such declarations, for example
`__STRATA_THREAD_REFS_DEFINED__`.

The intended ref meanings are:

* `StrongRef`: the caller owns a live reference and must release it according to
  the object API.
* `WeakRef`: the pointer may disappear unless it is acquired into a `StrongRef`.
* `BorrowedRef`: a short-lived non-owning view, usually returned by lookup or
  iteration APIs while the surrounding subsystem guarantees stability.
* `InternalRef`: intrusive kernel links and scheduler/object-private references
  that do not imply ownership.
* `LockedRef`: a borrowed view protected by a lock or equivalent synchronization
  contract; it must be released with the matching unlock API.

Ref-counted objects embed `struct StRefControlBlock ref_control` as their first
field. Strong/weak acquisition APIs should use the object-specific wrappers
rather than touching the control block directly outside the implementation.
Conversions between ref kinds should be explicit casts so the ownership boundary
is visible in review and to `folios-distinct-typedefs`.

MM allocation owners follow the same model: `StMm_AllocationOwner` is a
first-class ref-counted object, and VMM/PMM allocation records keep owner refs
while their allocations can remain live.

PMM allocation metadata uses `BorrowedRef` for unlocked read-only views and
`LockedRef` for metadata returned by `StPmm_LockAndGetAllocMetadata`.

### `folios-status-must-check`

Warns when a `StStatus` return value is silently discarded. Assign the result,
return it, use it in a condition, or write an explicit `(void)call()` when the
status is intentionally ignored.

## Usage

Build the plugin:

```sh
cmake --build build --target FoliosClangTidyPlugin
```

Run only the foliOS checks:

```sh
scripts/clang_tidy.sh \
  --build-dir build \
  --component strata \
  --folios-plugin build/tools/clang-tidy-folios/libFoliosClangTidyPlugin.so \
  --folios-checks
```
