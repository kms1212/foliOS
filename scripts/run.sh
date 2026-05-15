#!/usr/bin/env bash

set -euo pipefail

QEMU_ARCH=
QEMU_MACHINE=
OVMF_PATH=
CPU_TYPE=
BOOT_TYPE=
MEM_SIZE=128M
DISK_IMAGE=${FOLIOS_DISK_IMAGE:-disk.img}
FLOPPY_IMAGE=${FOLIOS_FLOPPY_IMAGE:-floppy.img}
CDROM_IMAGE=${FOLIOS_CDROM_IMAGE:-cdrom.iso}
CDBOOT=false
FDBOOT=false
declare -a DEVICES
declare -a QEMU_DEVICE_FLAGS
declare -a DRIVES
declare -a QEMU_DRIVE_FLAGS
declare -a QEMU_BASE_FLAGS
declare -a QEMU_EXTRA_ARGS

print_usage() {
    echo "usage: $0 [options] machine [qemu-args...]"
    echo
    echo "options:"
    echo "  -c, --cdboot          boot from the CD-ROM image"
    echo "      --cdrom path      CD-ROM image path (default: ${CDROM_IMAGE})"
    echo "  -d, --disk path       disk image path (default: ${DISK_IMAGE})"
    echo "  -f, --fdboot          boot from the floppy image"
    echo "      --floppy path     floppy image path (default: ${FLOPPY_IMAGE})"
    echo "  -h, --help            show this help"
    echo "  -m, --memory size     guest memory size (default: ${MEM_SIZE})"
    echo "  -u, --uefi            boot through UEFI firmware"
    echo
    echo "machine:"
    echo "  run '$0 help' to list available machine types"
}

print_machines() {
    echo "Available machine types:"
    echo "  isapc-ia32"
    echo "  q35-ia32"
    echo "  pc-ia32"
    echo "  isapc-amd64"
    echo "  q35-amd64"
    echo "  pc-amd64"
    echo "  virt-arm"
    echo "  virt-aarch64"
    echo "  mac99-ppc64"
}

require_option_value() {
    if [[ $# -lt 2 || "$2" == -* ]]; then
        echo "$0: missing value for $1" >&2
        exit 1
    fi
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -c | --cdboot)
            CDBOOT=true
            shift
            ;;
        --cdrom)
            require_option_value "$@"
            CDROM_IMAGE="$2"
            shift 2
            ;;
        -d | --disk)
            require_option_value "$@"
            DISK_IMAGE="$2"
            shift 2
            ;;
        -f | --fdboot)
            FDBOOT=true
            shift
            ;;
        --floppy)
            require_option_value "$@"
            FLOPPY_IMAGE="$2"
            shift 2
            ;;
        -h | --help)
            print_usage
            exit 0
            ;;
        -m | --memory)
            require_option_value "$@"
            MEM_SIZE="$2"
            shift 2
            ;;
        -u | --uefi)
            BOOT_TYPE=uefi
            shift
            ;;
        --)
            shift
            break
            ;;
        -*)
            print_usage
            exit 1
            ;;
        *)
            break
            ;;
    esac
done

if [[ $# -lt 1 ]]; then
    print_usage
    exit 1
fi

QEMU_MACHINE=$1
shift
QEMU_EXTRA_ARGS=("$@")

case $QEMU_MACHINE in
    isapc-ia32)
        QEMU_ARCH=ia32
        CPU_TYPE=486
        MACHINE_TYPE=isapc
        DEVICES=(
            "ide-hd,drive=fd0"
            isa-ide
            i8042
            ne2k_isa
            isa-vga
            sb16
            mc146818rtc
            pc-testdev
        )
        DRIVES=("file=${DISK_IMAGE},id=fd0,if=none,format=raw")
        if [ "$FDBOOT" = "true" ]; then
            DEVICES+=("floppy,drive=rd0")
            DRIVES+=("file=${FLOPPY_IMAGE},id=rd0,if=none,format=raw")
        fi
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("ide-cd,drive=rd1")
            DRIVES+=("file=${CDROM_IMAGE},id=rd1,if=none,format=raw")
        fi
        ;;
    q35-ia32)
        QEMU_ARCH=ia32
        MACHINE_TYPE=q35
        DEVICES=(
            "nvme,drive=fd0,serial=1234"
            intel-hda
            qemu-xhci
            ich9-usb-uhci6
            usb-ehci
            sdhci-pci
            sd-card
            am53c974
            e1000
            pci-serial
            pci-testdev
            usb-kbd
            usb-mouse
        )
        DRIVES=(
            "file=${DISK_IMAGE},id=fd0,if=none,format=raw"
        )
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("ide-cd,drive=rd0")
            DRIVES+=("file=${CDROM_IMAGE},id=rd0,if=none,format=raw")
        fi
        ;;
    pc-ia32)
        QEMU_ARCH=ia32
        MACHINE_TYPE=pc
        DEVICES=(
            "ide-hd,bus=ide.0,drive=fd0"
            intel-hda
            qemu-xhci
            ich9-usb-uhci6
            usb-ehci
            sdhci-pci
            sd-card
            am53c974
            e1000
            pci-serial
            pci-testdev
        )
        DRIVES=("file=${DISK_IMAGE},id=fd0,index=0,if=none,format=raw")
        if [ "$FDBOOT" = "true" ]; then
            DEVICES+=("floppy,drive=rd0")
            DRIVES+=("file=${FLOPPY_IMAGE},id=rd0,if=none,format=raw")
        fi
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("ide-cd,drive=rd1")
            DRIVES+=("file=${CDROM_IMAGE},id=rd1,if=none,format=raw")
        fi
        ;;
    isapc-amd64)
        QEMU_ARCH=x86_64
        MACHINE_TYPE=isapc
        DEVICES=(
            "ide-hd,drive=fd0"
            isa-ide
            i8042
            ne2k_isa
            isa-vga
            sb16
            mc146818rtc
            pc-testdev
        )
        DRIVES=("file=${DISK_IMAGE},id=fd0,if=none,format=raw")
        if [ "$FDBOOT" = "true" ]; then
            DEVICES+=("floppy,drive=rd0")
            DRIVES+=("file=${FLOPPY_IMAGE},id=rd0,if=none,format=raw")
        fi
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("ide-cd,drive=rd1")
            DRIVES+=("file=${CDROM_IMAGE},id=rd1,if=none,format=raw")
        fi
        ;;
    q35-amd64)
        QEMU_ARCH=x86_64
        MACHINE_TYPE=q35
        DEVICES=(
            "nvme,drive=fd0,serial=1234"
            intel-hda
            qemu-xhci
            ich9-usb-uhci6
            usb-ehci
            sdhci-pci
            sd-card
            am53c974
            e1000
            pci-serial
            pci-testdev
            usb-kbd
            usb-mouse
        )
        DRIVES=("file=${DISK_IMAGE},id=fd0,if=none,format=raw")
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("ide-cd,drive=rd0")
            DRIVES+=("file=${CDROM_IMAGE},id=rd0,if=none,format=raw")
        fi
        ;;
    pc-amd64)
        QEMU_ARCH=x86_64
        MACHINE_TYPE=pc
        DEVICES=(
            "ide-hd,drive=fd0"
            intel-hda
            qemu-xhci
            ich9-usb-uhci6
            usb-ehci
            sdhci-pci
            sd-card
            am53c974
            e1000
            pci-serial
            pci-testdev
        )
        DRIVES=("file=${DISK_IMAGE},id=fd0,if=none,format=raw")
        if [ "$FDBOOT" = "true" ]; then
            DEVICES+=("floppy,drive=rd0")
            DRIVES+=("file=${FLOPPY_IMAGE},id=rd0,if=none,format=raw")
        fi
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("ide-cd,drive=rd1")
            DRIVES+=("file=${CDROM_IMAGE},id=rd1,if=none,format=raw")
        fi
        ;;
    virt-arm)
        QEMU_ARCH=arm
        CPU_TYPE=max
        MACHINE_TYPE=virt
        DEVICES=(
            virtio-scsi-pci
            "scsi-hd,drive=fd0"
            virtio-gpu-pci
            intel-hda
            qemu-xhci
            ich9-usb-uhci6
            usb-ehci
            usb-kbd
            usb-mouse
            sdhci-pci
            sd-card
            e1000
            pci-serial
            pci-testdev
        )
        DRIVES=("file=${DISK_IMAGE},id=fd0,if=none,format=raw")
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("scsi-cd,drive=rd1")
            DRIVES+=("file=${CDROM_IMAGE},id=rd1,if=none,format=raw")
        fi
        ;;
    virt-aarch64)
        QEMU_ARCH=aarch64
        CPU_TYPE=max
        MACHINE_TYPE=virt
        DEVICES=(
            virtio-scsi-pci
            "scsi-hd,drive=fd0"
            virtio-gpu-pci
            intel-hda
            qemu-xhci
            ich9-usb-uhci6
            usb-ehci
            usb-kbd
            usb-mouse
            sdhci-pci
            sd-card
            e1000
            pci-serial
            pci-testdev
        )
        DRIVES=("file=${DISK_IMAGE},id=fd0,if=none,format=raw")
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("scsi-cd,drive=rd1")
            DRIVES+=("file=${CDROM_IMAGE},id=rd1,if=none,format=raw")
        fi
        ;;
    mac99-ppc64)
        QEMU_ARCH=ppc64
        MACHINE_TYPE=mac99
        DEVICES=(
            # "floppy,drive=rd0"
            am53c974
            "scsi-hd,drive=fd0"
            ES1370
            qemu-xhci
            ich9-usb-uhci6
            usb-ehci
            sdhci-pci
            sd-card
            e1000
            pci-serial
            pci-testdev
        )
        DRIVES=("file=${DISK_IMAGE},id=fd0,if=none,format=raw")
        if [ "$FDBOOT" = "true" ]; then
            DEVICES+=("floppy,drive=rd0")
            DRIVES+=("file=${FLOPPY_IMAGE},id=rd0,if=none,format=raw")
        fi
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("scsi-cd,drive=rd1")
            DRIVES+=("file=${CDROM_IMAGE},id=rd1,if=none,format=raw")
        fi
        ;;
    help)
        print_machines
        exit 0
        ;;
    *)
        print_machines
        exit 1
        ;;
esac

if [ "$BOOT_TYPE" = "uefi" ]; then
    case $QEMU_ARCH in
        ia32)
            OVMF_PATH="/usr/local/share/edk2.git/ovmf-ia32/OVMF-pure-efi.fd"
            ;;
        x86_64)
            OVMF_PATH="/usr/local/share/edk2.git/ovmf-x64/OVMF-pure-efi.fd"
            ;;
        arm)
            OVMF_PATH="/usr/local/share/edk2.git/arm/QEMU_EFI.fd"
            ;;
        aarch64)
            OVMF_PATH="/usr/local/share/edk2.git/aarch64/QEMU_EFI.fd"
            ;;
        *)
            echo "$0: unknown architecture"
            exit 1
            ;;
    esac
fi

for device in "${DEVICES[@]}"; do
    QEMU_DEVICE_FLAGS+=(-device "$device")
done

for drive in "${DRIVES[@]}"; do
    QEMU_DRIVE_FLAGS+=(-drive "$drive")
done

if [ "$CDBOOT" = "true" ]; then
    QEMU_EXTRA_ARGS+=(-boot d)
elif [ "$FDBOOT" = "true" ]; then
    QEMU_EXTRA_ARGS+=(-boot a)
fi

if [[ -n "${CPU_TYPE}" ]]; then
    QEMU_BASE_FLAGS+=(-cpu "${CPU_TYPE}")
fi
if [[ -n "${OVMF_PATH}" ]]; then
    QEMU_BASE_FLAGS+=(-bios "${OVMF_PATH}")
fi
QEMU_BASE_FLAGS+=(-m "${MEM_SIZE}" -M "${MACHINE_TYPE}" -s)

qemu-system-"${QEMU_ARCH}" \
    "${QEMU_BASE_FLAGS[@]}" \
    "${QEMU_DRIVE_FLAGS[@]}" \
    "${QEMU_DEVICE_FLAGS[@]}" \
    "${QEMU_EXTRA_ARGS[@]}"
