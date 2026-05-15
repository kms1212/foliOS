# foliOS Coding Style {#development_coding_style}

This document summarizes the coding style used by handwritten foliOS code after
reviewing the current `strata`, `vellum`, `common`, and SDK integration code.
It is the single source of truth for both naming and day-to-day C style.

The short version: code should make ownership, layer, and failure semantics
visible at the point where they matter. Do not hide policy in a generic helper,
global macro, or type name that is too broad for the concept it represents.

## Scope

These rules apply to handwritten kernel, bootloader, common loader, SIDL glue,
and foliOS SDK glue code.

Vendored or upstream-derived code, especially most of `folisdk/musl-strata`, may
keep its upstream style. foliOS-specific edits inside such code should still
respect the semantic rules here when practical, but do not churn unrelated
upstream formatting.

## Naming

Public functions use the namespace and layer encoded in the function name:

```text
St<Region>_<Action>(...)      // generic Strata subsystem
St<Region>A_<Action>(...)     // architecture-specific Strata backend
St<Region>P_<Action>(...)     // platform-specific Strata backend

Vl<Region>_<Action>(...)      // generic Vellum subsystem
Vl<Region>A_<Action>(...)     // architecture-specific Vellum backend
Vl<Region>P_<Action>(...)     // platform-specific Vellum backend
```

`Scope` is part of the architectural contract:

- `A` means CPU/ISA-specific code, usually under `arch/<arch>` or intrinsics.
- `P` means platform/board/firmware-specific code, usually under
  `arch/<arch>/<platform>`.
- no scope marker means generic subsystem logic.

The scope marker is appended to the region, not inserted before it:
`StThreadP_Switch`, `StMmP_MapLocalSparseMemory`, and `StApicA_SendEoi` are the
intended shape. Do not invent names such as `StPThread_*` or `StAThread_*`.

First-class object families may own their namespace even when they live under a
larger directory. Prefer `StAddressSpace_*`, `StAllocationOwner_*`, and
`StPool_*` over hiding those concepts behind a generic `StMm_*` prefix.

Static/private functions use `lower_snake_case`. Callback families may use a
short local prefix when it is part of a vtable or interface convention, such as
`prc_*` or `thr_*`.

Do not introduce a private helper whose name only hides a condition:

```c
// Prefer this when the condition is local policy.
if (!node || node->type != expected_type) return STATUS_INVALID_HANDLE;

// Avoid this unless "process node" has a reusable, non-trivial contract.
if (!is_process_node(node)) return STATUS_INVALID_HANDLE;
```

Keep a tiny helper only when it names a real operation or hardware primitive,
for example `read_hpet64`, `decode_module_cookie`, or an endian conversion. If
such a helper must stay separate and should compile away, use
`static __always_inline` in `.c` files or `__always_inline` in headers.

## Types

Global Strata types use `St...`; Vellum types use `Vl...`. Some Vellum object
types still use older lowercase names such as `struct device`; treat those as
migration targets, but do not rename them mechanically in unrelated patches.

Use `struct St<TypeName>` for ref-counted Strata object bodies:

```c
struct StThread;
struct StProcess;
struct StAddressSpace;
```

Reference typedefs are declared next to the object family, not in a central
`refs.h`:

```c
typedef struct StThread *StThread_StrongRef __ref_strong;
typedef struct StThread *StThread_WeakRef __ref_weak;
typedef struct StThread *StThread_BorrowedRef __ref_borrowed;
typedef struct StThread *StThread_InternalRef __ref_internal;
typedef struct StThread *StThread_LockedRef __ref_locked;
```

Use the suffixes exactly:

- `StrongRef`: owns a live reference and must be released.
- `WeakRef`: may disappear; acquire it before use.
- `BorrowedRef`: short non-owning view under a surrounding stability contract.
- `InternalRef`: intrusive links or subsystem-private references, not ownership.
- `LockedRef`: borrowed view produced by acquiring a lock, valid only while the
  matching lock is held and released by the matching unlock API.

Ref-counted objects embed `struct StRefControlBlock ref_control` as the first
field. Code outside the owning implementation should use object-specific
`Acquire` and `Release` wrappers rather than touching the control block.

Back links and intrusive lists should not create accidental strong cycles. Use
`InternalRef`, `BorrowedRef`, or `WeakRef` for non-owning relationships, and
make ownership transfers explicit.

Weak reference acquisition can fail because the object may already be dying or
reaped:

```c
StStatus StThread_AcquireWeak(
    StThread_WeakRef thread __in,
    StThread_StrongRef *threadout __out
);
```

Acquiring an already-owned `StrongRef` is infallible and should normally be
`void`. Acquiring from an `InternalRef` may return `StStatus` if the object can
be concurrently marked dying before it is promoted to `StrongRef`; otherwise it
must document the stronger synchronization guarantee that makes promotion
infallible.

Use `__nocast` for distinct numeric domains such as handles, ids, status codes,
pages, frames, and flags. Use `__bitwise` for endian-tagged integer types. Cast
at boundary helpers and macro constants, not at arbitrary call sites.

Flag typedefs should have a fixed-width integer base:

```c
typedef uint32_t StThread_CreateFlags __nocast;

#define TCF_DEFAULT  ((StThread_CreateFlags)0x00000000)
#define TCF_DETACHED ((StThread_CreateFlags)0x00000001)
```

## Public API Shape

Every public Strata/Vellum API parameter must carry a direction annotation,
including scalar values:

- `__in`: input parameter.
- `__out`: required output slot.
- `__inout`: required input/output slot.
- `__out_optional`: optional output slot.
- `__buf`: buffer pointer. It should be paired with a nearby size/count
  parameter until a richer form such as `__buf(count)` exists.

Function definitions should repeat the annotations so custom clang-tidy checks
can validate the implementation, not only the declaration.

Range parameters use two fixed shapes:

- `base` and `limit` describe an inclusive range: `[base, limit]`.
- any position parameter paired with `count` describes a half-open range from
  that position: `[position, position + count)`.

The position parameter does not need to be named `start`, `begin`, or `base`.
For example, `StMm_FreeGlobal(domain, vpn, count)` describes the virtual page
range `[vpn, vpn + count)`.

Do not use `limit` for an exclusive end. If a lower-level ABI or external format
already uses a different convention, convert at the boundary and document that
conversion locally.

For required outputs and in/out parameters, assert at entry:

```c
void StThread_GetCount(uint32_t *count __out)
{
    assert(count);
    ...
}
```

Do not use `if (!out)` handling for `__out` or `__inout`; that weakens the API
contract. Do not mechanically change such parameters to `__out_optional` to
silence a warning. Creation, allocation, open, and ownership-transfer functions
usually need a required output slot so the caller can receive and release the
resource.

A thin wrapper whose entire body is `return OtherApi(...);` may rely on the
callee's entry assert when the required output parameter is passed directly to a
callee parameter that is also `__out` or `__inout`. Do not use this exception
once the wrapper owns validation, cleanup, logging, transformation, or any other
local behavior.

For `__out_optional`, check for `NULL` before writing and do not assert.

For buffers, put the size/count parameter immediately after the buffer when the
ABI allows it:

```c
void StFoo_Read(uint8_t *buf __out __buf, size_t buf_size __in);
```

When a fixed ABI prevents that layout, use the nearest existing size/count
parameter and keep the relationship obvious in the parameter names. A future
annotation may grow this into an explicit `__buf(count)` form for static
analysis.

Use `StStatus` or `VlStatus` when an operation can fail in a way the caller can
handle. Use `void` for operations where failure is impossible, unrecoverable, or
not useful to the caller, especially `Free`, `Release`, `Unlock`, and simple
infallible setters/getters.

For getters, choose the shape by semantics:

- direct return is fine for naturally infallible values or nullable borrowed
  lookups;
- `void + __out` is preferred when an output slot makes repeated calls and
  ownership explicit;
- `StStatus + __out` is required when the lookup, conversion, or access can
  legitimately fail.

## Status Codes

`StStatus` is a typed status domain, not a generic integer. Always use
`CHECK_SUCCESS(status)` or `CHECK_FAILURE(status)` instead of comparing with
zero.

Every returned `StStatus` must be consumed. If intentionally ignored, write an
explicit cast:

```c
(void)StUtf_ConvertUtf8ToUtf32(...);
```

Use a local `StStatus status;` variable in multi-step functions, especially when
cleanup or ownership transfer is involved. Direct `return StFoo_Bar(...)` is
acceptable for thin wrappers and terminal propagation, but avoid long chains of
direct returns that obscure which resources are live.

Global `STATUS_*` codes in `status.h` should describe shared status meanings and
allocated status areas. Conversion policy belongs where the conversion happens.
For example, `STATUS_AREA_ACPI` may be global, but the conversion from
`uacpi_status` is local:

```c
#define MAKE_UACPI_STATUS(uacpi_status) \
    ((uacpi_status) ? MAKE_STATUS(MAKE_BASE_STATUS(1, uacpi_status, STATUS_AREA_ACPI), \
                                  STATUS_ATTR_NONE) \
                    : STATUS_SUCCESS)
```

Likewise, POSIX/C runtime process exit values should be converted by the runtime
exit path, not by a generic `STATUS_PROCESS_EXIT(code)` macro in `status.h`.

## Error Handling And Cleanup

For simple validation, return early:

```c
if (!node) return STATUS_INVALID_VALUE;
if (count == 0) return STATUS_INVALID_VALUE;
```

For multi-resource construction, initialize local resource variables to a safe
value and use a single `has_error:` label for failure cleanup:

```c
StStatus status;
StThread_StrongRef thread = NULL;
StAddressSpace_StrongRef asp = NULL;

status = StPool_AllocateClear(sizeof(*thread), (void **)&thread);
if (!CHECK_SUCCESS(status)) goto has_error;

status = StAddressSpace_Create(&asp, process);
if (!CHECK_SUCCESS(status)) goto has_error;

*threadout = thread;
return STATUS_SUCCESS;

has_error:
if (asp) StAddressSpace_Remove(asp);
if (thread) StPool_Free(thread);
return status;
```

If a resource must be released on both success and failure, keep the success
path and failure path visibly separate. `has_error:` is the failure path only;
do not fall through to it from success and do not `goto has_error` after setting
`status = STATUS_SUCCESS`.

```c
StStatus status;
int locked = 0;

status = StMutex_Lock(&lock);
if (!CHECK_SUCCESS(status)) return status;
locked = 1;

status = do_work();
if (!CHECK_SUCCESS(status)) goto has_error;

if (locked) StMutex_Unlock(&lock);
return STATUS_SUCCESS;

has_error:
if (locked) StMutex_Unlock(&lock);
return status;
```

Use boolean progress flags only when cleanup cannot be derived from a non-NULL
resource pointer or object state. Do not add flag variables just to appease the
analyzer if the value does not express real cleanup state.

If a failure is unrecoverable or would corrupt kernel invariants, panic rather
than returning a status the caller cannot handle.

## Ownership And Lifetime

Creation functions that return owned objects use `StStatus` and a required
`StrongRef * __out`. They initialize ownership before publishing the object to
global lists or GNT nodes.

`Acquire`/`Release` functions express reference ownership. A weak or internal
reference must be acquired into a strong reference before use when the object can
be reaped concurrently.

Remove/finalize paths are split when needed:

- begin removal marks the object as dying and detaches it from public lookup;
- finalization releases subresources once the last strong reference disappears;
- deferred reap is used when immediate destruction would violate scheduling or
  memory-pressure constraints.

Do not use `__out_optional` for object creation merely because a caller happens
to ignore the object today. Ignoring the output of a newly-created owned object
can leak the only handle to the resource unless the API separately transfers
ownership, such as a detached thread creation path.

## Include Order

Keep include groups separated by one blank line. The intended order is:

1. `config.h`, if the file needs generated configuration.
2. The implementation target header, if there is one.
3. Standard C library headers.
4. External library headers. If platform/architecture variants exist, include
   the selected platform/architecture header first.
5. Kernel or bootloader headers. Within this group, platform/architecture
   headers come before generic subsystem headers.
6. Common shared headers such as `loadst/*`.
7. Internal/private/generated local headers.

Examples:

```c
#include "config.h"

#include <strata/thread.h>

#include <assert.h>
#include <stdint.h>

#include <strata/plat/thread.h>

#include <strata/compiler.h>
#include <strata/status.h>

#include <loadst/bootinfo.h>

#include "internal.h"
```

```c
#include <stdint.h>
#include <string.h>

#include <vellum/plat/bios/bootinfo.h>
#include <vellum/plat/bios/disk.h>

#include <vellum/compiler.h>
#include <vellum/status.h>

#include "../../filesystem/fat/fat.h"
```

Headers must be self-contained and guarded. Use existing guard style:
`__STRATA_..._H__`, `__VELLUM_..._H__`, or `__LOADST_..._H__`. Do not use
`#pragma once`.

Prefer including the narrowest header that owns the declaration. Do not create a
central refs header for unrelated object families.

## File Layout

Use this order for `.c` files:

1. includes;
2. `MODULE_NAME`, local constants, local conversion macros;
3. private types;
4. file-scope state;
5. forward declarations, when needed;
6. small private helpers;
7. public or vtable-facing implementation;
8. constructors/destructors or registration blocks.

`MODULE_NAME` should be present in files that log through module-aware logging
macros.

Keep policy conversion macros close to the code that owns the policy. Local
macros like `MAKE_UACPI_STATUS` and `MAKE_PROCESS_EXIT_STATUS` are preferred to
generic macros in public headers when the meaning is subsystem-specific.

## Formatting

Formatting follows the root `.clang-format`:

- 4-space indentation, no tabs.
- 100-column limit.
- K&R control blocks, with function opening braces on the next line.
- Pointer stars bind to the variable: `uint8_t *buf`.
- Short guard-return `if` statements may stay on one line when there is no
  `else`.
- Empty functions may stay on one line only when clang-format leaves them that
  way.
- Align related macro definitions when it improves scanning; do not align local
  variable declarations manually.

Prefer one declaration per semantic item when cleanup or ownership matters:

```c
StStatus status;
StThread_StrongRef thread = NULL;
StProcess_StrongRef process = NULL;
```

Compact declarations are acceptable for tightly coupled scalar temporaries in
existing bootloader code that has not been migrated yet, but avoid making
resource lifetime harder to audit.

## Comments

Comments should explain invariants, ownership, memory ordering, ABI contracts,
hardware behavior, or non-obvious cleanup. Avoid comments that restate the next
line of code.

Public or cross-component declarations in headers should use Doxygen comments
as the canonical reference text. Keep generated reference documentation thin:
the contract should live next to the declaration it describes.

Header comments should cover the details a caller cannot infer from the type
alone:

- ownership, reference type, and release responsibility;
- nullability that is not already obvious from annotations;
- locking, preemption, interrupt, or panic-path constraints;
- status codes and expected failure modes;
- resource lifetime and cleanup responsibilities;
- struct field invariants when fields are visible outside the owning file.

Do not document every trivial scalar field. For internal structs, document the
groups of fields that carry an invariant, such as intrusive list links, refcount
state, stack ownership, mapping policy, or hardware-visible layout.

Good comments in this codebase usually explain why a path exists:

- panic/interrupt/symbol lookup constraints;
- ref-count or reap ordering;
- page table and demand-paging semantics;
- firmware or hardware quirks;
- intentional clang-tidy suppressions.

Use `NOLINT(...)` only for a concrete structural reason and name the check.

## Architecture And Platform Boundaries

Architecture intrinsics should be small `__always_inline` wrappers around CPU
instructions or register access. Keep them in `arch/<arch>/include/.../intrinsics`
or a similarly narrow location.

Generic code should call generic APIs unless it is explicitly implementing an
architecture or platform backend. Do not use an intrinsic as an example of a
generic subsystem API.

Platform hooks should sit at the platform boundary. For example, page-table bit
details and demand-paging PTE flags belong in the platform MM mapping path or
VMM/MM contract, not in arbitrary call sites.

## Memory Management

Use typed page/frame/count values:

- `St_PhysFrame` for physical frame numbers;
- `St_VirtPage` for virtual page numbers;
- `St_PageCount` for page-sized counts. PMM code may use it as a frame count,
  while VMM/MM code usually uses it as a page count;
- `uintptr_t` for byte addresses and address arithmetic.

Use the provided conversion macros such as `ADDR_TO_PAGE`, `PAGE_TO_ADDR`,
`ADDR_TO_FRAME`, and `FRAME_TO_ADDR`.

Allocation flags and mapping flags are separate typed domains. Do not mix
`StMm_AllocFlags` and `StMm_MapFlags` without an explicit boundary conversion.

Demand paging policy belongs in MM/VMM and platform mapping paths. Call sites
should express desired mapping semantics with typed flags such as `MF_IMMEDIATE`
or `MF_ZERO_FILL`; they should not patch PTE bits directly.

Free/unmap functions are `void` unless the caller can do something meaningful
with a failure. Validate invariants internally with asserts or panic if
corruption is detected.

## Synchronization

Use capability annotations on locks where available:

```c
StStatus StMutex_Lock(struct StMutex *mtx) __acquires(mtx);
void StMutex_Unlock(struct StMutex *mtx) __releases(mtx);
```

Preemption and scheduler state changes should have explicit lock/unlock calls.
Do not hide lock ownership in names that look like pure queries.

Atomic variables are used for counters and CPU-local state. Prefer the narrowest
memory order that matches the invariant, but leave a comment when the ordering
is part of the correctness argument.

## Vellum Migration

Vellum is being migrated toward the same style. New Vellum code and touched
Vellum code should move in this direction instead of preserving old patterns by
default. The same naming shape applies with `Vl`:

- `VlA_*` for architecture code;
- `VlP_*` and firmware-specific prefixes such as `VlBiosP_*` for platform code;
- `VlDev_*`, `VlFs_*`, `VlShell_*`, and similar subsystem APIs for generic code.

Driver registration uses constructor macros such as `REGISTER_DEVICE_DRIVER` and
`REGISTER_SHELL_COMMAND`; keep the local init function small and registration
near the vtable/static object it publishes.

Vellum status handling uses `VlStatus` and `CHECK_SUCCESS`/`CHECK_FAILURE` just
like Strata. Public Vellum APIs should use direction annotations from
`<vellum/compiler.h>` as they are migrated.

## Tooling

Before considering a code cleanup done, run the relevant build and checks:

```sh
ninja -C build strata -j 4
ninja -C build vellum -j 4
git diff --check
```

For the foliOS-specific clang-tidy plugin, the important checks are:

- `folios-api-annotations`: public API parameters need annotations;
- `folios-api-nullability`: annotation contracts must match null handling;
- `folios-distinct-typedefs`: `__nocast`, `__bitwise`, and ref typedefs have
  type meaning;
- `folios-status-must-check`: `StStatus` results must be checked or explicitly
  ignored.

Use `(void)` for intentionally ignored statuses, not an unused temporary.
