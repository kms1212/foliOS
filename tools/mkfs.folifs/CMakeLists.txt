cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)

project(mkfs.folifs)

# Compiler Tests

# Dependencies

# Target Configurations
add_executable(mkfs.folifs)

# Compile Options
target_compile_features(mkfs.folifs PUBLIC c_std_11)
target_compile_features(mkfs.folifs PUBLIC cxx_std_14)
target_compile_options(mkfs.folifs PUBLIC 
    $<$<COMPILE_LANGUAGE:C,CXX>:
        -Werror
        -Wall
        -Wno-unused-function
        -Wno-unused-variable
        -Wno-unused-but-set-variable
        -pedantic
        -pedantic-errors
    >
)
target_sources(mkfs.folifs PRIVATE main.c crc32.c)
target_compile_definitions(mkfs.folifs PRIVATE CRC32_FAST)

target_include_directories(mkfs.folifs PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(mkfs.folifs PRIVATE "${CMAKE_BINARY_DIR}")

# Additional Targets

# Subdirectories
