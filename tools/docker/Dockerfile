FROM lordmilko/i686-elf-tools:latest

# Install build dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    ninja-build \
    python3 \
    python3-pip \
    mtools \
    gdisk \
    uuid-runtime \
    bison \
    flex \
    git \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /work
