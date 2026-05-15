# Vellum Modules {#vellum_modules}

This page describes module discovery, loading order, dependency metadata, and
handoff records.

## Module Object

Vellum's current module object records:

- next module link;
- opened ELF file;
- module name;
- load VPN;
- loaded program size.

`VlModule_Load` loads a module by path, `VlModule_Unload` releases it,
`VlModule_GetFirst` returns the loaded list, and `VlModule_Find` searches by
name.

## Loading Contract

Module loading is a bootloader concern until Strata takes ownership. Vellum
should load module bytes, record where they were placed, and describe them in
handoff metadata. Kernel policy about module trust, runtime isolation, GNT
registration, or process/module object creation belongs in Strata.

## Ordering

If dependencies are introduced, the ordering rule should be explicit in the
module metadata rather than encoded in filesystem traversal order. Until then,
loading order should remain simple and reproducible.
