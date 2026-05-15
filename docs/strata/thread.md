# Thread {#strata_thread}

This page describes Strata thread objects, kernel and user stacks, scheduling
state, wait/detach behavior, and teardown.

## Object Model

`struct StThread` is a ref-counted object. It contains:

- intrusive links for scheduler, mutex blocking, and process membership;
- id, type, state, detach/main-thread flags;
- a strong process reference;
- platform thread data;
- kernel stack metadata;
- optional user stack metadata;
- wait/sleep state;
- scheduler fairness/runtime counters.

The thread object is not a raw handle to memory that can disappear at any time.
APIs that need a stable thread use `StThread_StrongRef`; scheduler and process
intrusive links use `StThread_InternalRef`.

## Creation Flags

Thread creation uses `StThread_CreateFlags`:

- `TCF_DEFAULT`: normal joinable thread.
- `TCF_DETACHED`: thread is born detached, so the creator does not receive or
  hold the join reference.

The detached-at-creation path avoids the race where a thread can finish and be
reaped before the creator calls a separate detach operation.

## Wait, Detach, Remove

`StThread_Detach` transfers away the join reference. It is valid for a thread
that has already finished; if cleanup is needed, it requests scheduler
maintenance instead of treating "already finished" as a caller error.

`StThread_Wait` waits for every joinable thread in the wait list to finish, or
for the timeout path to set `STATUS_TIMER_EXPIRED`. Use
`THREAD_WAIT_INFINITE` for an unbounded wait; callers should not pass `-1`
directly.

`StThread_Remove` is stricter: the thread must be finished unless the caller is
on a path that explicitly owns the teardown invariant. Finished detached
threads are normally reaped by scheduler maintenance or deferred reap.

## Stacks

Kernel threads get kernel stacks in a global kernel domain. User threads get a
user stack in the process address space. Current user stacks use
`MF_USER_DEFAULT | MF_GUARD | MF_GUARD_GROW_DOWN`, so stack pages can be
materialized lazily and grow downward through the guard mechanism.

## Exit

`StThread_Exit` marks the current thread finished, requests scheduler
maintenance, and yields forever. It is `__noreturn` because returning after a
thread has declared itself finished would violate scheduler and stack lifetime
rules.
