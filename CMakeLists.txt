cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(CheckLanguage)
include(CheckTypeSize)
include(ExternalProject)
# include(ProcessTarget)
# include(BuildConfiguration)

project("foliOS")

# Load build configurations
# load_build_configuration(TARGET CONFIG_PRESET)

# Subdirectories
add_subdirectory(tools)  # host tools

# Project configurations
set(TARGET "ia32-pc-bios" CACHE STRING "Target triplet (arch-board-firmware)")
set(CONFIG_PRESET "default" CACHE STRING "Preset name of build configuration")
option(ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
option(BUILD_TESTING "Build tests" OFF)

string(REPLACE "-" ";" TARGET_TEMP ${TARGET})
list(LENGTH TARGET_TEMP TARGET_TEMP_LEN)
if (NOT TARGET_TEMP_LEN EQUAL 3)
    message(FATAL_ERROR "Invalid target format")
endif()
list(GET TARGET_TEMP 0 TARGET_ARCH)
list(GET TARGET_TEMP 1 TARGET_PLATFORM)
list(GET TARGET_TEMP 2 TARGET_FIRMWARE)
unset(TARGET_TEMP)

if ("${TARGET_ARCH}" STREQUAL "amd64")
    set(VELLUM_TARGET_ARCH ia32)
else()
    set(VELLUM_TARGET_ARCH "${TARGET_ARCH}")
endif()

if (ENABLE_CLANG_TIDY)
    if ("${TARGET_ARCH}" STREQUAL "amd64")
        set(VELLUM_C_CLANG_TIDY "clang-tidy;--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy;--extra-arg=-D__CLANG_TIDY__;--extra-arg=--target=i686-elf")
    else()
        set(VELLUM_C_CLANG_TIDY "clang-tidy;--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy;--extra-arg=-D__CLANG_TIDY__;--extra-arg=--target=${VELLUM_TARGET_ARCH}-elf")
    endif()

    set(STRATA_C_CLANG_TIDY "clang-tidy;--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy;--extra-arg=-D__CLANG_TIDY__;--extra-arg=--target=${TARGET_ARCH}-elf;--extra-arg=-Wno-gnu-include-next")
endif()

# vellum (bootloader)
ExternalProject_Add(vellum
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/vellum"
    BINARY_DIR "${CMAKE_BINARY_DIR}/vellum"
    CMAKE_CACHE_ARGS
        -DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}
        -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS}
        -DCMAKE_ASM_FLAGS:STRING=${CMAKE_ASM_FLAGS}
        -DCMAKE_C_CLANG_TIDY:STRING=${VELLUM_C_CLANG_TIDY}
        -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=${CMAKE_EXPORT_COMPILE_COMMANDS}
        -DCMAKE_COMPILER_LAUNCHER:FILEPATH=${CMAKE_COMPILER_LAUNCHER}
        -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_SOURCE_DIR}/cmake/${VELLUM_TARGET_ARCH}/Toolchain.cmake
    CMAKE_ARGS
        -DTARGET_ARCH=${VELLUM_TARGET_ARCH}
        -DTARGET_PLATFORM=${TARGET_PLATFORM}
        -DTARGET_FIRMWARE=${TARGET_FIRMWARE}
        -DROOT_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DROOT_BINARY_DIR=${CMAKE_BINARY_DIR}
        -DBUILD_TESTING=${BUILD_TESTING}
    BUILD_ALWAYS 1
    INSTALL_COMMAND ""
)

# strata (kernel)
ExternalProject_Add(strata
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/strata"
    BINARY_DIR "${CMAKE_BINARY_DIR}/strata"
    CMAKE_CACHE_ARGS
        -DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}
        -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS}
        -DCMAKE_ASM_FLAGS:STRING=${CMAKE_ASM_FLAGS}
        -DCMAKE_C_CLANG_TIDY:STRING=${STRATA_C_CLANG_TIDY}
        -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=${CMAKE_EXPORT_COMPILE_COMMANDS}
        -DCMAKE_COMPILER_LAUNCHER:FILEPATH=${CMAKE_COMPILER_LAUNCHER}
        -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_SOURCE_DIR}/cmake/${TARGET_ARCH}/Toolchain.cmake
    CMAKE_ARGS
        -DTARGET_ARCH=${TARGET_ARCH}
        -DTARGET_PLATFORM=${TARGET_PLATFORM}
        -DTARGET_FIRMWARE=${TARGET_FIRMWARE}
        -DROOT_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DROOT_BINARY_DIR=${CMAKE_BINARY_DIR}
        -DBUILD_TESTING=${BUILD_TESTING}
    BUILD_ALWAYS 1
    INSTALL_COMMAND ""
)
