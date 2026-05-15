# Annotations {#development_annotations}

This page describes API direction annotations, reference annotations,
`__nocast`, `__bitwise`, and how local clang-tidy checks interpret them.

## Direction Annotations

Every public API parameter should carry a direction annotation, including scalar
parameters:

- `__in`: input parameter.
- `__out`: required output slot.
- `__inout`: required input/output slot.
- `__out_optional`: optional output slot.
- `__buf`: buffer pointer, paired with an obvious size/count parameter.

Function definitions should repeat the annotations from declarations. The
custom checks inspect implementations as well as public headers.

Required output slots are contracts. `__out` and `__inout` should assert at
entry; they should not be handled with a recoverable `if (!out)` branch. Use
`__out_optional` only when dropping the output cannot leak ownership or hide a
required result.

## Buffer Shape

Until a richer annotation such as `__buf(count)` exists, put the size/count
parameter immediately after the buffer whenever the ABI allows it:

```c
void StFoo_Read(uint8_t *buf __out __buf, size_t buf_size __in);
```

When an ABI prevents that layout, keep the relationship clear in parameter
names.

## Reference Annotations

Reference annotations describe object lifetime:

- `__ref_strong`: owned live reference.
- `__ref_weak`: weak reference that must be acquired before use.
- `__ref_borrowed`: non-owning view protected by another contract.
- `__ref_internal`: intrusive or subsystem-private reference.
- `__ref_locked`: borrowed view valid while the matching lock is held.

They are paired with typedef names such as `StThread_StrongRef`; code should not
depend on naming alone.

## Distinct Numeric Domains

Use `__nocast` for values that share a C representation but are not
interchangeable: status codes, handles, pages, frames, flags, and object ids.
Use `__bitwise` for endian-tagged integer domains. Cast at boundary helpers and
macro constants, not at arbitrary call sites.
