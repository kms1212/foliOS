#!/bin/bash

clang-query \
    -p build/strata \
    --extra-arg="--target=x86_64-elf" \
    --extra-arg="-D__x86_64__" \
    --extra-arg="-DCRC32_FAST=1" \
    --extra-arg="-ffreestanding" \
    --extra-arg="-fno-stack-protector" \
    --extra-arg="-mno-red-zone" \
    --extra-arg="-mxsave" \
    --extra-arg="-Werror" \
    --extra-arg="-Wall" \
    --extra-arg="-Wno-unused-function" \
    --extra-arg="-Wno-unused-variable" \
    --extra-arg="-Wno-unused-but-set-variable" \
    --extra-arg="-pedantic" \
    --extra-arg="-pedantic-errors" \
    --extra-arg="-I/Users/gwonminsu/Documents/LocalProjects/foliOS/strata" \
    --extra-arg="-I/Users/gwonminsu/Documents/LocalProjects/foliOS/strata/include" \
    --extra-arg="-I/Users/gwonminsu/Documents/LocalProjects/foliOS/build/strata" \
    --extra-arg="-I/Users/gwonminsu/Documents/LocalProjects/foliOS/common/include" \
    --extra-arg="-I/Users/gwonminsu/Documents/LocalProjects/foliOS/strata/lib/liballoc/include" \
    --extra-arg="-I/Users/gwonminsu/Documents/LocalProjects/foliOS/strata/lib/rb/include" \
    --extra-arg="-I/Users/gwonminsu/Documents/LocalProjects/foliOS/strata/arch/amd64/pc/include" \
    --extra-arg="-I/Users/gwonminsu/Documents/LocalProjects/foliOS/build/strata/arch/amd64/pc/trampoline" \
    --extra-arg="-I/Users/gwonminsu/Documents/LocalProjects/foliOS/strata/arch/amd64/include" \
    --extra-arg="-I/Users/gwonminsu/Documents/LocalProjects/foliOS/strata/arch/amd64" \
    --extra-arg="-isystem /Users/gwonminsu/Documents/LocalProjects/foliOS/build/strata/_deps/zlib-src" \
    --extra-arg="-isystem /Users/gwonminsu/Documents/LocalProjects/foliOS/build/strata/_deps/zlib-build" \
    strata/mm/mm.c \
    --
