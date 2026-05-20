# Vellum Boot Flow {#vellum_boot_flow}

This page describes Vellum stages, firmware interaction, disk loading, and
handoff to the kernel.

## Responsibilities

Vellum's boot flow is responsible for:

- entering from the firmware-specific startup path;
- initializing basic console/debug output;
- discovering memory and marking unavailable ranges;
- opening the boot filesystem;
- reading boot configuration;
- loading bootloader modules and assets;
- running the `loadmodule` command for `load_folios`;
- providing the initialized environment that lets `load_folios` collect handoff data
  and jump to Strata.

Platform mechanics belong under the Vellum architecture/platform directories.
This document describes the cross-component contract.

## Configuration

The generic main path reads `boot:/config/boot.json` when available. The
configuration can provide values such as password, timezone, RTC mode, and start
script commands. Missing configuration should leave safe defaults rather than
blocking the loader unnecessarily.

## Console Setup

Vellum sets up a usable terminal by composing device interfaces: video,
framebuffer, virtual console, ANSI terminal, keyboard, and standard streams.
This makes boot diagnostics visible before Strata logging is available.

## Handoff

The handoff to Strata is owned by the `load_folios` bootloader module. Vellum core
should expose the services and device/configuration data needed by that module,
but the kernel-facing table should use common `stload` structures rather than
Vellum-private data.

The current contract is documented in
[stload Handoff ABI](../common/stload-abi.md). Changes to `load_folios` handoff
behavior should keep that ABI document and the shared `stload` headers in sync.
