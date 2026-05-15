# Disk Images {#development_disk_images}

This page describes disk image generation and bootloader architecture options.

For current BIOS boot images, `scripts/mkdisk.sh -a ia32 disk.img` is expected
because the bootloader architecture is IA-32.

## Typical Flow

```sh
(cd folisdk && ./build.py --arch x86_64 --builddir-layout --jobs 18)
export PATH="$(pwd)/folisdk/build/folisdk-host/bin:$(pwd)/folisdk/build/folisdk-x86_64/bin:$PATH"

folisdk/build/folisdk-host/bin/cmake \
    -S . \
    -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DTARGET=amd64-pc-bios \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
folisdk/build/folisdk-host/bin/cmake --build build --parallel=18
scripts/mkdisk.sh -a ia32 disk.img
scripts/run.sh --disk disk.img pc-amd64
```

The `-a` option names the bootloader architecture, not the kernel architecture.
For `amd64-pc-bios`, Strata is AMD64 while the BIOS Vellum path is IA-32.

## Running A Specific Image

`run.sh` no longer assumes that the boot disk must be named `disk.img`:

```sh
scripts/run.sh --disk build/test.img pc-amd64
scripts/run.sh --disk disk.img --memory 256M pc-amd64
scripts/run.sh --disk disk.img pc-amd64 -debugcon stdio
```

The machine name ends script option parsing. Arguments after the machine name
are passed directly to QEMU.

CD-ROM and floppy paths are configurable too:

```sh
scripts/run.sh --cdboot --cdrom cdrom.iso --disk disk.img pc-amd64
scripts/run.sh --fdboot --floppy floppy.img --disk disk.img pc-ia32
```

## Boot Configuration

The disk image should contain the boot configuration needed by Vellum to find
the kernel and modules. Prefer fixing configuration in the image over adding
manual QEMU workarounds; it keeps test runs reproducible.

## Related Tools

- `scripts/mkdisk.sh`: build a bootable disk image.
- `scripts/mkcdrom.sh`: build an ISO-style boot medium when supported.
- `scripts/run.sh`: run a target in QEMU with configurable image paths.
- `scripts/monitor.sh`: run QEMU in tmux with debug log, optional GDB, and
  optional CPU/memory plots.
- `tools/folifsimg` and `tools/mkfs.folifs`: create and inspect foliOS
  filesystem images.
