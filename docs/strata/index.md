# Strata {#strata}

Strata documents describe kernel implementation contracts and subsystem design.

- @subpage strata_memory "Memory"
- @subpage strata_process "Process"
- @subpage strata_thread "Thread"
- @subpage strata_scheduler "Scheduler"
- @subpage strata_gnt "Global Node Tree"
- @subpage strata_symbols "Symbols"

## Conventions

Strata public APIs use the `St<Region>_<Action>` naming pattern. Architecture
and platform backends append the scope marker to the region, for example
`StThreadP_Switch` or `StSyscallA_Handler`.

Most first-class objects are ref-counted and expose type-specific reference
typedefs. Subsystems should prefer explicit ownership and checked status flow
over hidden global state.

## Component Reference

The generated Strata reference is built separately from Vellum and common
headers. Use this conceptual section to understand subsystem contracts, then use
the generated reference for exact declarations.
