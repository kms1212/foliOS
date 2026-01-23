#!/usr/bin/env bash

set -e

ARCH=
BOOTBIN_EXT=
OUTPUT=
PRESERVE_TEMP=false
CURRENT_IMAGE=
BOOT_IMAGE=

print_usage() {
    echo "usage: $0 [-a arch] [-hS] output"
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
        *)
            print_usage
            exit 1
            ;;
    esac
done

case $ARCH in
    ia32)
        BOOTBIN_EXT=X86
        ;;
    amd64)
        BOOTBIN_EXT=X64
        ;;
    *)
        echo "$0: unknown architecture"
        exit 1
        ;;
esac

shift "$((OPTIND - 1))"
OUTPUT=$1

# El Torito floppy image
CURRENT_IMAGE=$(mktemp)
BOOT_IMAGE=$CURRENT_IMAGE
dd if=/dev/zero of="$CURRENT_IMAGE" bs=512 count=2880
mformat -i "$CURRENT_IMAGE" -B "build/vellum/arch/$ARCH/pc/bios/fdboot.bin"
mcopy -i "$CURRENT_IMAGE" "build/vellum/arch/$ARCH/pc/bios/stage1.bin" ::/STAGE1.$BOOTBIN_EXT
mcopy -i "$CURRENT_IMAGE" "build/vellum/arch/$ARCH/pc/bios/vellum.bin" ::/VELLUM.$BOOTBIN_EXT

if [ "${PRESERVE_TEMP}" = true ]; then
    cp "$CURRENT_IMAGE" "${OUTPUT%.*}.fd.img";
fi

if [ -d "./.mkcdrom.temp" ]; then
    rm -r ./.mkcdrom.temp
fi
mkdir ./.mkcdrom.temp
mkdir ./.mkcdrom.temp/config
mkdir ./.mkcdrom.temp/modules
cp "$BOOT_IMAGE" ./.mkcdrom.temp/boot.img
cp vellum/config/boot.json ./.mkcdrom.temp/config/boot.json
cp build/vellum/modules/loadst/loadst.mod ./.mkcdrom.temp/modules/loadst.mod
cp build/vellum/modules/helloworld/helloworld.mod ./.mkcdrom.temp/modules/helowrld.mod
cp build/vellum/bootloader.map ./.mkcdrom.temp/bootldr.map
cp build/vellum/unifont.bfn ./.mkcdrom.temp/unifont.bfn
cp disk/plchldr.bmp ./.mkcdrom.temp/plchldr.bmp
cp disk/unicode.txt ./.mkcdrom.temp/unicode.txt

mkdir ./.mkcdrom.temp/system
mkdir ./.mkcdrom.temp/system/kernel
mkdir ./.mkcdrom.temp/system/lib
mkdir ./.mkcdrom.temp/system/subsys
mkdir ./.mkcdrom.temp/system/services
mkdir ./.mkcdrom.temp/system/drivers
cp build/strata/strata.elf ./.mkcdrom.temp/system/kernel/strata.elf

xorriso -as mkisofs -o "$OUTPUT" -b "/boot.img" ./.mkcdrom.temp

rm -r ./.mkcdrom.temp

rm "$BOOT_IMAGE"
