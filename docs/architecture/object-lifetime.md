# Object Lifetime {#architecture_object_lifetime}

Object lifetime in Strata uses explicit reference types and a small intrusive
control block. Public API names and typedef annotations expose the model so
lifetime bugs can be reviewed and eventually checked by tooling.

## Reference Kinds

Ref-counted object families declare their reference typedefs next to the object
family:

```c
typedef struct StThread *StThread_StrongRef __ref_strong;
typedef struct StThread *StThread_WeakRef __ref_weak;
typedef struct StThread *StThread_BorrowedRef __ref_borrowed;
typedef struct StThread *StThread_InternalRef __ref_internal;
typedef struct StThread *StThread_LockedRef __ref_locked;
```

The suffixes carry meaning:

- `StrongRef` owns a live reference and must be released.
- `WeakRef` may disappear; acquiring it can fail.
- `BorrowedRef` is a non-owning view under a surrounding stability contract.
- `InternalRef` is for intrusive links or subsystem-private references, not
  ownership transfer.
- `LockedRef` is a borrowed view valid only while the matching lock is held.

## Control Block

Ref-counted objects embed `struct StRefControlBlock ref_control` as their first
field. Object-specific `Acquire` and `Release` wrappers are the public
interface; code outside the owning implementation should not manipulate the
control block directly.

The control block tracks a reference count, object pointer, finalize callback,
dying state, reap-queued state, and deferred page count. Placing it first allows
reference code to find the control block without a second allocation and
supports typed weak-to-strong promotion.

## Removal Phases

Objects that are visible from global lookup or scheduler structures usually need
more than a single `free` step:

- begin removal marks the object dying and removes it from public lookup;
- outstanding strong references continue to protect live readers;
- finalization releases owned subresources after the final reference drops;
- deferred reap handles cases where immediate destruction would conflict with
  scheduler, interrupt, or memory-pressure constraints.

This is especially important for process-thread relationships. A process may
own references to its threads while a thread refers back to its process. Those
relationships must be modeled with the correct reference kind so a strong cycle
does not keep dead objects alive forever.

## API Consequences

Creation functions that publish an owned object should return `StStatus` and
write a required `StrongRef * __out`. Detach-style APIs may transfer ownership
to the scheduler or process model, but the transfer must be explicit in the API
contract. Do not make creation outputs optional just because one current caller
does not need the handle.

APIs that consume a caller-owned strong reference should take the reference
slot, not a copy of the pointer. Mark unconditional consumers `__nullized`; mark
status-returning consumers that only clear on success `__success_nullized`.
Clearing the slot makes stale post-release use visible in review and to local
clang-tidy checks.
