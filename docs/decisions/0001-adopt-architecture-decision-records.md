# 0001: Adopt Architecture Decision Records {#decision_0001_adopt_architecture_decision_records}

Status: accepted

Date: 2026-05-15

## Context

foliOS has several design choices that affect API design, subsystem boundaries,
and static analysis rules. Some of those choices already exist in the codebase,
while new decisions are still being made during development.

Keeping those decisions only in code, commit messages, or discussion history
makes it hard for a new contributor to tell which patterns are intentional and
which ones are transitional.

## Decision

Use Architecture Decision Records for design decisions that affect public API,
cross-subsystem contracts, memory ownership, static analysis, boot flow, or
debugging behavior.

ADR numbers are assigned in documentation order, not original project history.
Older decisions may be documented later as retrospective ADRs when doing so
clarifies current code or future work.

## Consequences

The ADR series begins with the introduction of the documentation system. New
records can be added immediately, while retrospective ADRs can document older
decisions when useful.

New records should stay short, describe the accepted direction, and link to
conceptual documentation when the decision needs more background.
