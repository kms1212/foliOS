# Target Profiles {#architecture_targets}

Target profiles describe how the generic foliOS architecture maps onto a
concrete build and boot environment. Each profile documents CPU entry state,
firmware assumptions, bootloader architecture, platform devices, and
architecture-local protection backends.

- @subpage architecture_target_amd64_pc_bios "amd64-pc-bios"
- @subpage architecture_target_ia32_pc_bios_loader "IA-32 PC BIOS Loader"

A profile should not redefine subsystem contracts. The canonical contracts
remain in the relevant architecture, common ABI, Strata, Vellum, SDK, and ADR
documents. A profile explains which of those contracts are active for a
specific target configuration.

Build target names use the `arch-platform-firmware` format. For example,
`amd64-pc-bios` means AMD64 Strata, the PC platform layer, and BIOS boot
firmware. The bootloader architecture may still be different from the kernel
architecture when the firmware path requires it.
