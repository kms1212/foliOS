# Vellum Modules {#vellum_modules}

This page describes Vellum module discovery, loading order, and dependency
metadata.

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

Module loading is a bootloader concern until handoff. Vellum should load module
bytes and record where they were placed. Runtime policy about module trust,
isolation, GNT registration, or process/module object creation belongs on the
consumer side of the handoff boundary.

## Ordering

If dependencies are introduced, the ordering rule should be explicit in the
module metadata rather than encoded in filesystem traversal order. Until then,
loading order should remain simple and reproducible.
