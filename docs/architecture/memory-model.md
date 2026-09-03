# Memory Model {#architecture_memory_model}

The memory model is split into explicit layers so each operation's effect is
clear: reserving address space, allocating physical backing, mapping
caller-owned frames, or resolving a lazy mapping.

## Address-Space Scopes

Strata uses two broad virtual-memory scopes:

- Global VMM domains describe kernel-wide regions such as fast kernel memory,
  slower kernel memory, I/O mappings, modules, and KRT-global mappings.
- Local domains belong to `StAddressSpace` objects and describe user process
  address spaces.

Global operations take an `enum StVmm_Domain`. Local operations take an
`StAddressSpace_StrongRef`. The type-level split is intentional: it keeps local
page faults, user stack growth, and process-owned allocation cleanup from
looking like generic kernel mappings.

## Lifecycles

The public MM layer is organized around three lifecycles:

- `Map -> Unmap`: attach caller-provided physical frames to virtual memory.
- `Allocate -> Free`: acquire virtual address space and physical backing
  together.
- `Reserve -> Commit -> Free`: reserve virtual address space and policy first,
  then attach physical backing later.

`Allocate` returns usable memory. It may materialize backing immediately or use
an internal on-demand policy. `Reserve` records virtual address space and policy
without implying that physical frames exist yet.

## Demand And Guard Policy

Lazy local mappings are represented as VMM mapping policy, not as ad hoc PTE
patches at call sites. For user memory, `MF_USER_DEFAULT` includes
`MF_ZERO_FILL`, and non-immediate local sparse/image mappings can be resolved by
the page-fault path.

Guard pages are mapping policy too. `MF_GUARD` reserves an inaccessible page
adjacent to the usable range. `MF_GUARD_GROW_DOWN` is currently a local-only
guard subpolicy used for downward-growing user stacks; it requires demand-zero
mapping and cannot be combined with `MF_IMMEDIATE`.

## Typed Units

The code uses typed domains for units that must not be mixed accidentally:

- `St_PhysFrame`: physical frame number;
- `St_VirtPage`: virtual page number;
- `St_PageCount`: count of page-sized units. PMM paths interpret it as a frame
  count, and VMM/MM paths interpret it as a page count;
- `StMm_AllocFlags`: allocation placement and alignment policy;
- `StMm_MapFlags`: mapping protection and mapping behavior.

The clang-tidy annotations model `St_PhysFrame` and `St_VirtPage` as page-unit
indexes in separate physical and virtual domains, and `St_PageCount` as the
matching page-unit distance. Indexes can move by counts, and subtracting indexes
within the same domain yields a count; virtual and physical index domains do not
mix implicitly.

Use conversion helpers such as `ADDR_TO_PAGE`, `PAGE_TO_ADDR`,
`ADDR_TO_FRAME`, and `FRAME_TO_ADDR` at boundaries. Do not smuggle virtual page
numbers, physical frame numbers, page/frame counts, and byte addresses through
untyped integers.
