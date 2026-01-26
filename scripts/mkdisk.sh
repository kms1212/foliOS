#!/usr/bin/env bash

set -e

ARCH=
BOOTBIN_EXT=
UEFI_ARCH=
BOOT_TYPE=bios
OUTPUT=
PRESERVE_TEMP=false
PART_TABLE_IMAGE=
CURRENT_PART_IMAGE=
declare -a PART_IMAGES

print_usage() {
    echo "usage: $0 [-a arch] [-hSu] output"
}

while getopts "a:hSu" arg; do
    case $arg in
        a)
            ARCH=$OPTARG
            ;;
        h)
            print_usage
            exit 0
            ;;
        S)
            PRESERVE_TEMP=true
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

case $ARCH in
    ia32)
        BOOTBIN_EXT=X86
        UEFI_ARCH=IA32
        ;;
    amd64)
        BOOTBIN_EXT=X64
        UEFI_ARCH=X64
        ;;
    *)
        echo "$0: unknown architecture"
        exit 1
        ;;
esac

case $BOOT_TYPE in
    uefi) ;;
    bios) ;;
    *)
        echo "$0: unknown boot type"
        exit 1
        ;;
esac

shift "$((OPTIND - 1))"
OUTPUT=$1


# Partition 0
CURRENT_PART_IMAGE=$(mktemp)
PART_IMAGES+=("$CURRENT_PART_IMAGE")
dd if=/dev/zero of="$CURRENT_PART_IMAGE" bs=512 count=16443
case $BOOT_TYPE in
    uefi)
        mformat -i "$CURRENT_PART_IMAGE" -H 63
        mmd -i "$CURRENT_PART_IMAGE" ::/EFI
        mmd -i "$CURRENT_PART_IMAGE" ::/EFI/BOOT
        mcopy -i "$CURRENT_PART_IMAGE" "build/vellum/arch/$ARCH/pc/uefi/vellum.efi" ::/EFI/BOOT/BOOT$UEFI_ARCH.EFI
        ;;
    bios)
        mformat -i "$CURRENT_PART_IMAGE" -H 63 -B "build/vellum/arch/$ARCH/pc/bios/fdboot.bin"
        mcopy -i "$CURRENT_PART_IMAGE" "build/vellum/arch/$ARCH/pc/bios/stage1.bin" ::/STAGE1.$BOOTBIN_EXT
        mcopy -i "$CURRENT_PART_IMAGE" "build/vellum/arch/$ARCH/pc/bios/vellum.bin" ::/VELLUM.$BOOTBIN_EXT
        ;;
esac
mmd -i "$CURRENT_PART_IMAGE" ::/CONFIG
mcopy -i "$CURRENT_PART_IMAGE" vellum/config/boot.json ::/CONFIG/boot.json
mmd -i "$CURRENT_PART_IMAGE" ::/MODULES
mcopy -i "$CURRENT_PART_IMAGE" build/vellum/modules/loadst/loadst.mod ::/MODULES/LOADST.MOD
mcopy -i "$CURRENT_PART_IMAGE" build/vellum/modules/guishell/guishell.mod ::/MODULES/guishell.MOD
mcopy -i "$CURRENT_PART_IMAGE" build/vellum/modules/helloworld/helloworld.mod ::/MODULES/HELOWRLD.MOD
mcopy -i "$CURRENT_PART_IMAGE" build/vellum/vellum.map ::/VELLUM.MAP
mcopy -i "$CURRENT_PART_IMAGE" build/vellum/unifont.bfn ::/UNIFONT.BFN
mcopy -i "$CURRENT_PART_IMAGE" disk/plchldr.bmp ::/PLCHLDR.BMP
mcopy -i "$CURRENT_PART_IMAGE" disk/unicode.txt ::/UNICODE.TXT

if [ "${PRESERVE_TEMP}" = true ]; then
    cp "$CURRENT_PART_IMAGE" "${OUTPUT%.*}.part0.img";
fi


# Partition 1
CURRENT_PART_IMAGE=$(mktemp)
PART_IMAGES+=("$CURRENT_PART_IMAGE")
dd if=/dev/zero of="$CURRENT_PART_IMAGE" bs=512 count=65520
build/tools/mkfs.folifs/mkfs.folifs -l "Label" -j 1M -N 2 -r 1 -s 2 -o 8253 -u "$(uuidgen)" -iv "$CURRENT_PART_IMAGE"
# build/tools/folifsimg/folifsimg mkdir -i "$CURRENT_PART_IMAGE" :/system
# build/tools/folifsimg/folifsimg mkdir -i "$CURRENT_PART_IMAGE" :/system/kernel
# build/tools/folifsimg/folifsimg mkdir -i "$CURRENT_PART_IMAGE" :/system/lib
# build/tools/folifsimg/folifsimg mkdir -i "$CURRENT_PART_IMAGE" :/system/subsys
# build/tools/folifsimg/folifsimg mkdir -i "$CURRENT_PART_IMAGE" :/system/services
# build/tools/folifsimg/folifsimg mkdir -i "$CURRENT_PART_IMAGE" :/system/drivers
# build/tools/folifsimg/folifsimg mkdir -i "$CURRENT_PART_IMAGE" :/users
# build/tools/folifsimg/folifsimg mkdir -i "$CURRENT_PART_IMAGE" :/users/root
# build/tools/folifsimg/folifsimg mkdir -i "$CURRENT_PART_IMAGE" :/packages
# build/tools/folifsimg/folifsimg mkdir -i "$CURRENT_PART_IMAGE" :/temp
# build/tools/folifsimg/folifsimg copy -i "$CURRENT_PART_IMAGE" build/strata/strata.elf :/system/strata/strata.elf

if [ "${PRESERVE_TEMP}" = true ]; then
    cp "$CURRENT_PART_IMAGE" "${OUTPUT%.*}.part1.img";
fi


# Partition 2
CURRENT_PART_IMAGE=$(mktemp)
PART_IMAGES+=("$CURRENT_PART_IMAGE")
dd if=/dev/zero of="$CURRENT_PART_IMAGE" bs=512 count=131040
mformat -i "$CURRENT_PART_IMAGE" -H 73773 -F
mmd -i "$CURRENT_PART_IMAGE" ::/system
mmd -i "$CURRENT_PART_IMAGE" ::/system/kernel
mmd -i "$CURRENT_PART_IMAGE" ::/system/lib
mmd -i "$CURRENT_PART_IMAGE" ::/system/subsys
mmd -i "$CURRENT_PART_IMAGE" ::/system/services
mmd -i "$CURRENT_PART_IMAGE" ::/system/drivers
mmd -i "$CURRENT_PART_IMAGE" ::/users
mmd -i "$CURRENT_PART_IMAGE" ::/users/root
mmd -i "$CURRENT_PART_IMAGE" ::/packages
mmd -i "$CURRENT_PART_IMAGE" ::/temp
mcopy -i "$CURRENT_PART_IMAGE" build/strata/strata.elf ::/system/kernel/strata.elf

if [ "${PRESERVE_TEMP}" = true ]; then
    cp "$CURRENT_PART_IMAGE" "${OUTPUT%.*}.part2.img";
fi


# Partition Table
PART_TABLE_IMAGE=$(mktemp)
case $BOOT_TYPE in
    uefi)
        dd if=/dev/zero of="$PART_TABLE_IMAGE" bs=512 count=63
        cat "$PART_TABLE_IMAGE" "${PART_IMAGES[@]}" > "$OUTPUT"
        cat <<'EOF' | gdisk "$OUTPUT"
o
y
x
l
63
m
n
1
63
16505
ef00
n
2
16506
82025
cd7cdb25-ee47-55dc-9989-6dfd81ef7261
n
3
82026
213066
bce7d2e7-c1d5-573f-a5d8-7a8b40081b81
w
y
EOF
        ;;
    bios)
        cp "build/vellum/arch/$ARCH/pc/bios/mbrboot.bin" "$PART_TABLE_IMAGE"
        dd if=/dev/zero bs=512 count=62 >>"$PART_TABLE_IMAGE"
        cat "$PART_TABLE_IMAGE" "${PART_IMAGES[@]}" > "$OUTPUT"

        if [[ "$OSTYPE" == "linux-gnu"* ]]; then
            # Linux (using sfdisk)
            cat <<-EOF | sfdisk "$OUTPUT"
label: dos
label-id: 0x0
device: $OUTPUT
unit: sectors

$OUTPUT1 : start=63, size=16443, type=1, bootable
$OUTPUT2 : start=16506, size=65520, type=78
$OUTPUT3 : start=82026, size=131040, type=79
EOF
        else
            # MacOS / BSD (using fdisk -e)
            cat <<-EOF | fdisk -e "$OUTPUT"
e 1
01
n
63
16443
f 1
e 2
78
n
16506
65520
edit 3
79
n
82026
131040
q
EOF
        fi
        ;;
    *)  ;;
esac

if [ "${PRESERVE_TEMP}" = true ]; then
    cp "$PART_TABLE_IMAGE" "${OUTPUT%.*}.pt.img";
fi


rm "$PART_TABLE_IMAGE" "${PART_IMAGES[@]}"
