#!/usr/bin/env bash
set -e

# Configuration
PREFIX="$HOME/opt/cross"
TARGETS=("i686-elf" "x86_64-elf")
BINUTILS_VERSION="2.43"
GCC_VERSION="14.2.0"
CORES=$(nproc)

echo "Building toolchain for targets: ${TARGETS[*]}"
echo "Installing to: $PREFIX"
echo "Using: Binutils $BINUTILS_VERSION, GCC $GCC_VERSION"
echo "Parallel jobs: $CORES"

# Create prefix directory
mkdir -p "$PREFIX"
export PATH="$PREFIX/bin:$PATH"

# Create source directory
mkdir -p build_toolchain_tmp
cd build_toolchain_tmp

# Download sources
if [ ! -f "binutils-$BINUTILS_VERSION.tar.xz" ]; then
    echo "Downloading Binutils..."
    wget "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz"
fi

if [ ! -f "gcc-$GCC_VERSION.tar.xz" ]; then
    echo "Downloading GCC..."
    wget "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz"
fi

# Extract sources
if [ ! -d "binutils-$BINUTILS_VERSION" ]; then
    echo "Extracting Binutils..."
    tar -xf "binutils-$BINUTILS_VERSION.tar.xz"
fi

if [ ! -d "gcc-$GCC_VERSION" ]; then
    echo "Extracting GCC..."
    tar -xf "gcc-$GCC_VERSION.tar.xz"
fi

# Build for each target
for TARGET in "${TARGETS[@]}"; do
    echo "=== Building for $TARGET ==="

    # 1. Build Binutils
    echo "Configuring Binutils for $TARGET..."
    mkdir -p "build-binutils-$TARGET"
    cd "build-binutils-$TARGET"
    ../binutils-$BINUTILS_VERSION/configure --target="$TARGET" --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
    
    echo "Building Binutils for $TARGET..."
    make -j"$CORES"
    
    echo "Installing Binutils for $TARGET..."
    make install
    cd ..

    # 2. Build GCC
    echo "Configuring GCC for $TARGET..."
    mkdir -p "build-gcc-$TARGET"
    cd "build-gcc-$TARGET"
    ../gcc-$GCC_VERSION/configure --target="$TARGET" --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
    
    echo "Building GCC for $TARGET..."
    make -j"$CORES" all-gcc
    make -j"$CORES" all-target-libgcc
    
    echo "Installing GCC for $TARGET..."
    make install-gcc
    make install-target-libgcc
    cd ..
done

echo "Toolchain build complete!"
cd ..
rm -rf build_toolchain_tmp
