# foliOS Documentation {#mainpage}

foliOS is an experimental operating system built around Strata, a kernel that
explores the Ambikernel model: conventional process isolation where it is still
the right tool, combined with hardware-assisted low-overhead domains for kernel
modules and privileged runtime paths. The documentation should make a clear
distinction between implemented contracts, active migration work, and longer
term architecture goals.

## Guide

- @subpage architecture "Architecture"
- @subpage strata "Strata kernel"
- @subpage vellum "Vellum bootloader"
- @subpage common "Common shared interfaces"
- @subpage sdk "SDK and runtime"
- @subpage development "Development"
- @subpage decisions "Design decisions"

## Reference

The component references are generated separately so Strata, Vellum, and common
loader contracts do not collapse into one hard-to-scan type index.

- [Strata API reference](../strata/html/index.html)
- [Vellum API reference](../vellum/html/index.html)
- [Common API reference](../common/html/index.html)

The top-level Doxygen target only contains conceptual documentation. Component
targets include their corresponding headers and component-local markdown.

## Policy

Write design intent and operational contracts in this tree. The markdown files
are included in the Doxygen input set so conceptual design notes and generated
API reference can be browsed from the same documentation build. Generated
reference output belongs under the build directory, normally under
`build/docs/doxygen`.

Avoid documenting aspirational features as if they are already complete. When a
feature is still a research direction, say so directly and point readers to the
current implementation boundary.
