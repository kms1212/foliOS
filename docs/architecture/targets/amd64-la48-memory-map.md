# AMD64 LA48 Memory Map {#architecture_target_amd64_la48_memory_map}

This page documents the AMD64 48-bit linear-address layout used by the current
PC profile. The source of truth for active constants is
`strata/arch/amd64/pc/include/strata/plat/memmap.h`; this page gives the human
view of those constants and the reserved layout bands around them.

All `base` and `limit` ranges in this page are inclusive: `[base, limit]`.
Ranges described by a `count` elsewhere in the MM/VMM APIs use count semantics,
for example `[vpn, vpn + count)`.

## Canonical Shape

LA48 has two canonical halves:

- lower canonical half: `0x0000_0000_0000_0000` through
  `0x0000_7FFF_FFFF_FFFF`;
- non-canonical hole: `0x0000_8000_0000_0000` through
  `0xFFFF_7FFF_FFFF_FFFF`;
- upper canonical half: `0xFFFF_8000_0000_0000` through
  `0xFFFF_FFFF_FFFF_FFFF`.

Strata treats the lower half as local address-space territory plus KRT data,
and the upper half as global Strata-controlled territory.

## Layout

| Start | End | Region | Notes |
| --- | --- | --- | --- |
| `0x0000_0000_0000_0000` | `0x0000_0000_001F_FFFF` | Low local reserved area | Kept outside `MEMMAP_USER_*`; catches null and low-address mistakes. |
| `0x0000_0000_0020_0000` | `0x0000_7FFF_7FFF_FFFF` | User area | Local `StAddressSpace` VMM domain for user code, data, heap, and stacks. |
| `0x0000_7FFF_8000_0000` | `0x0000_7FFF_FFFF_FFFF` | KRT data area | Lower-half KRT data mapping. |
| `0x0000_8000_0000_0000` | `0xFFFF_7FFF_FFFF_FFFF` | Non-canonical hole | Not a valid LA48 canonical address range. |
| `0xFFFF_8000_0000_0000` | `0xFFFF_8000_7FFF_FFFF` | KRT text area | Upper-half KRT text and read-only runtime entry mapping. |
| `0xFFFF_8000_8000_0000` | `0xFFFF_BFFF_7FFF_FFFF` | Module area | Global module image and module-owned memory region. |
| `0xFFFF_BFFF_8000_0000` | `0xFFFF_BFFF_FFFF_FFFF` | Module guard/reserved gap | Reserved gap before direct mapping. |
| `0xFFFF_C000_0000_0000` | `0xFFFF_C7FF_7FFF_FFFF` | Direct mapping area | Kernel physical-direct map window. |
| `0xFFFF_C7FF_8000_0000` | `0xFFFF_C7FF_FFFF_FFFF` | Direct mapping guard/reserved gap | Reserved gap before PMM metadata. |
| `0xFFFF_C800_0000_0000` | `0xFFFF_CFFF_FFFF_FFFF` | Memory frame metadata area | PMM frame metadata window. |
| `0xFFFF_D000_0000_0000` | `0xFFFF_EFFF_FFFF_FFFF` | Reserved global area | Available for future global layout expansion. |
| `0xFFFF_F000_0000_0000` | `0xFFFF_F7FF_FFFF_FFFF` | I/O mapping area | Global MMIO and device mapping domain. |
| `0xFFFF_F800_0000_0000` | `0xFFFF_FFFE_FFFF_FFFF` | Kernel slow area | General global kernel allocation domain. |
| `0xFFFF_FFFF_0000_0000` | `0xFFFF_FFFF_1FFF_FFFF` | Kernel per-thread area | Reserved high-kernel subarea. |
| `0xFFFF_FFFF_2000_0000` | `0xFFFF_FFFF_3FFF_FFFF` | Kernel per-process area | Reserved high-kernel subarea. |
| `0xFFFF_FFFF_4000_0000` | `0xFFFF_FFFF_5FFF_FFFF` | Kernel per-CPU area | Reserved high-kernel subarea. |
| `0xFFFF_FFFF_6000_0000` | `0xFFFF_FFFF_7FFF_FFFF` | Kernel trampoline area | Reserved for trampoline and high-kernel transition use. |
| `0xFFFF_FFFF_8000_0000` | `0xFFFF_FFFF_FFFF_FFFF` | Kernel fast area | Linked kernel fast region and fast global VMM domain tail. |

The linked kernel image starts in the kernel fast area. During initialization,
the fast VMM domain begins after the linked kernel image and extends through
`MEMMAP_KERNEL_FAST_ADDR_LIMIT`.

## VMM Domain Mapping

Current global VMM domains map onto the layout as follows:

- `VMM_DOMAIN_KERNEL_FAST`: kernel fast area after the linked kernel image;
- `VMM_DOMAIN_KERNEL_SLOW`: kernel slow area;
- `VMM_DOMAIN_IO`: I/O mapping area;
- `VMM_DOMAIN_MODULE`: module area;
- `VMM_DOMAIN_KRT_GLOBAL`: KRT text area.

Local `StAddressSpace` instances use `MEMMAP_USER_ADDR_BASE` through
`MEMMAP_USER_ADDR_LIMIT` for user memory. KRT data lives in the lower canonical
half but is not part of an ordinary user allocation domain.
