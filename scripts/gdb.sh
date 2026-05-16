#!/usr/bin/env bash

set -e

GDB_ARCH=
TARGET=
declare -a OBJECTS
declare -a GDB_OBJECT_FLAGS

print_usage() {
    echo "usage: $0 [-h] arch"
}

while getopts "hit:" arg; do
    case $arg in
        h)
            print_usage
            exit 0
            ;;
        t)
            TARGET=$OPTARG
            ;;
        *)
            print_usage
            exit 1
            ;;
    esac
done

shift "$((OPTIND - 1))"

case $1 in
    ia32)
        GDB_ARCH=i386
        case $TARGET in
            vellum)
                OBJECTS=(
                    "build/vellum/arch/ia32/pc/bios/fdboot.elf"
                    "build/vellum/arch/ia32/pc/bios/stage1.elf"
                )
                ;;
            strata)
                ;;
        esac
        ;;
    amd64)
        GDB_ARCH=x86_64
        case $TARGET in
            vellum)
                OBJECTS=(
                    "build/vellum/arch/ia32/pc/bios/fdboot.elf"
                    "build/vellum/arch/ia32/pc/bios/stage1.elf"
                )
                ;;
            strata)
                OBJECTS=(
                    "build/strata/arch/amd64/pc/trampoline/trampoline.elf"
                    "build/strata/arch/amd64/pc/krt/krt.elf"
                    "build/syspkgs_temp/SystemManager/main.app"
                )
                ;;
        esac
        ;;
esac

case $TARGET in
    vellum)
        OBJECTS+=("build/vellum/vellum.elf")
        ;;
    strata)
        OBJECTS+=("build/strata/strata.elf")
        ;;
esac

for object in "${OBJECTS[@]}"; do
    GDB_OBJECT_FLAGS+=(--eval-command="add-symbol-file $object")
done

shift
"$GDB_ARCH-elf-gdb" \
    --eval-command="set confirm off" \
    "${GDB_OBJECT_FLAGS[@]}" \
    --eval-command="set confirm on" \
    --command="./.gdbinit" \
    "$@"
