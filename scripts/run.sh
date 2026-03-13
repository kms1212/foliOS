#!/usr/bin/env bash

set -e

QEMU_ARCH=
QEMU_MACHINE=
OVMF_PATH=
CPU_TYPE=
BOOT_TYPE= 
MEM_SIZE=128M
CDBOOT=false
FDBOOT=false
declare -a ADDITIONAL_FLAGS
declare -a DEVICES
declare -a QEMU_DEVICE_FLAGS
declare -a DRIVES
declare -a QEMU_DRIVE_FLAGS

print_usage() {
    echo "usage: $0 [-cfhu] machine"
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

while getopts "cfhu" arg; do
    case $arg in
        c)
            CDBOOT=true
            ;;
        f)
            FDBOOT=true
            ;;
        h)
            print_usage
            exit 0
            ;;
        u)
            BOOT_TYPE=uefi
            ;;
        *)
            print_usage
            exit 1
            ;;
    esac
done

shift "$((OPTIND - 1))"
QEMU_MACHINE=$1

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
        DRIVES=("file=disk.img,id=fd0,if=none,format=raw")
        ADDITIONAL_FLAGS=(-debugcon stdio)
        if [ "$FDBOOT" = "true" ]; then
            DEVICES+=("floppy,drive=rd0")
            DRIVES+=("file=floppy.img,id=rd0,if=none,format=raw")
        fi
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("ide-cd,drive=rd1")
            DRIVES+=("file=cdrom.iso,id=rd1,if=none,format=raw")
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
            "file=disk.img,id=fd0,if=none,format=raw"
        )
        ADDITIONAL_FLAGS=(-debugcon stdio)
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("ide-cd,drive=rd0")
            DRIVES+=("file=cdrom.iso,id=rd0,if=none,format=raw")
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
        DRIVES=("file=disk.img,id=fd0,index=0,if=none,format=raw")
        ADDITIONAL_FLAGS=(-debugcon stdio)
        if [ "$FDBOOT" = "true" ]; then
            DEVICES+=("floppy,drive=rd0")
            DRIVES+=("file=floppy.img,id=rd0,if=none,format=raw")
        fi
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("ide-cd,drive=rd1")
            DRIVES+=("file=cdrom.iso,id=rd1,if=none,format=raw")
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
        DRIVES=("file=disk.img,id=fd0,if=none,format=raw")
        ADDITIONAL_FLAGS=(-debugcon stdio)
        if [ "$FDBOOT" = "true" ]; then
            DEVICES+=("floppy,drive=rd0")
            DRIVES+=("file=floppy.img,id=rd0,if=none,format=raw")
        fi
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("ide-cd,drive=rd1")
            DRIVES+=("file=cdrom.iso,id=rd1,if=none,format=raw")
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
        DRIVES=("file=disk.img,id=fd0,if=none,format=raw")
        ADDITIONAL_FLAGS=(-debugcon stdio)
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("ide-cd,drive=rd0")
            DRIVES+=("file=cdrom.iso,id=rd0,if=none,format=raw")
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
        DRIVES=("file=disk.img,id=fd0,if=none,format=raw")
        ADDITIONAL_FLAGS=(-debugcon stdio)
        if [ "$FDBOOT" = "true" ]; then
            DEVICES+=("floppy,drive=rd0")
            DRIVES+=("file=floppy.img,id=rd0,if=none,format=raw")
        fi
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("ide-cd,drive=rd1")
            DRIVES+=("file=cdrom.iso,id=rd1,if=none,format=raw")
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
        DRIVES=("file=disk.img,id=fd0,if=none,format=raw")
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("scsi-cd,drive=rd1")
            DRIVES+=("file=cdrom.iso,id=rd1,if=none,format=raw")
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
        DRIVES=("file=disk.img,id=fd0,if=none,format=raw")
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("scsi-cd,drive=rd1")
            DRIVES+=("file=cdrom.iso,id=rd0,if=none,format=raw")
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
        DRIVES=("file=disk.img,id=fd0,if=none,format=raw")
        if [ "$FDBOOT" = "true" ]; then
            DEVICES+=("floppy,drive=rd0")
            DRIVES+=("file=floppy.img,id=rd0,if=none,format=raw")
        fi
        if [ "$CDBOOT" = "true" ]; then
            DEVICES+=("scsi-cd,drive=rd1")
            DRIVES+=("file=cdrom.iso,id=rd1,if=none,format=raw")
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
        amd64)
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
    ADDITIONAL_FLAGS+=(-boot d)
elif [ "$FDBOOT" = "true" ]; then
    ADDITIONAL_FLAGS+=(-boot a)
fi

# shellcheck disable=2086
shift
qemu-system-$QEMU_ARCH \
    ${CPU_TYPE:+-cpu $CPU_TYPE} \
    ${OVMF_PATH:+-bios $OVMF_PATH} \
    -m $MEM_SIZE \
    -M $MACHINE_TYPE \
    -s \
    "${QEMU_DRIVE_FLAGS[@]}" \
    "${QEMU_DEVICE_FLAGS[@]}" \
    "${ADDITIONAL_FLAGS[@]}" \
    "$@"
