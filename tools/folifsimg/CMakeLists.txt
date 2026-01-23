cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)

project(folifsimg)

# Compiler Tests

# Dependencies

# Target Configurations
add_executable(folifsimg)

# Compile Options
target_compile_features(folifsimg PUBLIC c_std_11)
target_compile_features(folifsimg PUBLIC cxx_std_14)
target_compile_options(folifsimg PUBLIC 
    $<$<COMPILE_LANGUAGE:C,CXX>:
        -Werror
        -Wall
        -Wno-unused-function
        -Wno-unused-variable
        -Wno-unused-but-set-variable
        -Wno-unused-private-field
    >
)
target_sources(folifsimg PRIVATE folifs.cpp crc32.c image.cpp main.cpp)
target_compile_definitions(folifsimg PRIVATE CRC32_FAST)

target_include_directories(folifsimg PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(folifsimg PRIVATE "${CMAKE_BINARY_DIR}")

# Additional Targets

# Subdirectories
