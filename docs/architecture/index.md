# Architecture {#architecture}

Architecture documents describe the system shape before individual subsystem
details. They should explain why a boundary exists and which invariants other
layers rely on.

- @subpage architecture_ambikernel "Ambikernel"
- @subpage architecture_boot_flow "Boot Flow"
- @subpage architecture_memory_model "Memory Model"
- @subpage architecture_object_lifetime "Object Lifetime"
- @subpage architecture_targets "Target Profiles"

Use this section for cross-cutting contracts. Subsystem-specific mechanics live
under @ref strata "Strata", @ref vellum "Vellum", and @ref sdk "SDK and
Runtime".
