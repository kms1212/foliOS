# Scheduler {#strata_scheduler}

This page describes scheduler queues, runnable state, preemption contracts, and
the relationship between scheduler-owned references and thread lifetime.

## Data Model

Scheduler state is stored in per-CPU scheduler data:

- runqueue head/tail;
- current thread;
- context-switch count;
- idle runtime accounting;
- maintenance interval/request state.

The runqueue uses `StThread_InternalRef` links. Scheduler code does not own
ordinary strong references for every queue link; it relies on the thread
lifetime protocol around creation, detach, finish, and reap.

## Selection

Runnable selection uses a simple fairness key:

- `sched_pass` is the monotonically increasing pass value;
- `sched_run_count` tracks how many slices selected the thread;
- the runnable thread with the lowest pass is selected.

Pending threads become running when first scheduled. Sleeping and waiting
threads become runnable when their deadline or wait condition is satisfied.

## Maintenance

Maintenance runs periodically or when requested. It removes finished detached
threads that are safe to reap, handles follow-up work after the current thread
finishes, and coordinates deferred cleanup under page-pressure budgets.

If a thread finishes while another execution path or CPU still holds a strong
reference, maintenance should mark or queue the object rather than freeing the
referenced object.

## Preemption

Code that mutates scheduler-visible lists should hold the appropriate
preemption/interrupt exclusion. Do not hide scheduler state changes in helpers
that look like pure queries; switching and removal are lifetime-sensitive.
