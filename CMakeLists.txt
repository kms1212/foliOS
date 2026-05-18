cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(CheckLanguage)
include(CheckTypeSize)
include(ExternalProject)
include(ProcessTarget)
include(BuildConfiguration)

project("foliOS")

# Subdirectories
add_subdirectory(tools)  # host tools

# Project configurations
set(TARGET "ia32-pc-bios" CACHE STRING "Target triplet (arch-board-firmware)")
set(CONFIG_PRESET "default" CACHE STRING "Preset name of build configuration")
option(ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
option(BUILD_TESTING "Build tests" OFF)

process_target(TARGET)
load_build_configuration("${TARGET_CONFIG_ARCH}" "${TARGET_PLATFORM}" "${TARGET_FIRMWARE}" "${CONFIG_PRESET}")
load_component_build_configuration(strata)
get_build_configuration_cache_args(BUILD_CONFIGURATION_CACHE_ARGS)

if ("${TARGET_ARCH}" STREQUAL "amd64")
    set(VELLUM_TARGET_ARCH ia32)
else()
    set(VELLUM_TARGET_ARCH "${TARGET_ARCH}")
endif()

add_subdirectory(docs)
add_subdirectory(syspkgs_temp)

if (ENABLE_CLANG_TIDY)
    if ("${TARGET_ARCH}" STREQUAL "amd64")
        set(VELLUM_C_CLANG_TIDY "clang-tidy;--quiet;--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy;--extra-arg=-D__CLANG_TIDY__;--extra-arg=--target=i686-elf")
    else()
        set(VELLUM_C_CLANG_TIDY "clang-tidy;--quiet;--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy;--extra-arg=-D__CLANG_TIDY__;--extra-arg=--target=${VELLUM_TARGET_ARCH}-elf")
    endif()

    set(STRATA_C_CLANG_TIDY "clang-tidy;--quiet;--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy;--extra-arg=-D__CLANG_TIDY__;--extra-arg=--target=${TARGET_ARCH}-elf;--extra-arg=-Wno-gnu-include-next")
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
        -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_SOURCE_DIR}/cmake/vellum/${VELLUM_TARGET_ARCH}/Toolchain.cmake
        ${BUILD_CONFIGURATION_CACHE_ARGS}
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
        -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_SOURCE_DIR}/cmake/strata/${TARGET_ARCH}/Toolchain.cmake
        ${BUILD_CONFIGURATION_CACHE_ARGS}
    CMAKE_ARGS
        -DTARGET_ARCH=${TARGET_ARCH}
        -DTARGET_PLATFORM=${TARGET_PLATFORM}
        -DTARGET_FIRMWARE=${TARGET_FIRMWARE}
        -DROOT_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DROOT_BINARY_DIR=${CMAKE_BINARY_DIR}
        -DSTRATA_USEREXEC_APP=${FOLIOS_SYSTEM_MANAGER_APP}
        -DBUILD_TESTING=${BUILD_TESTING}
    BUILD_ALWAYS 1
    INSTALL_COMMAND ""
)

# Static Analysis (clang-tidy)
add_custom_target(
    tidy-strata
    COMMAND "${CMAKE_SOURCE_DIR}/scripts/clang_tidy.sh" --build-dir "${CMAKE_BINARY_DIR}" --component strata
    DEPENDS strata
    USES_TERMINAL
    COMMENT "Running clang-tidy for strata"
)

add_custom_target(
    tidy-vellum
    COMMAND "${CMAKE_SOURCE_DIR}/scripts/clang_tidy.sh" --build-dir "${CMAKE_BINARY_DIR}" --component vellum
    DEPENDS vellum
    USES_TERMINAL
    COMMENT "Running clang-tidy for vellum"
)

add_custom_target(
    tidy-all
    COMMAND "${CMAKE_SOURCE_DIR}/scripts/clang_tidy.sh" --build-dir "${CMAKE_BINARY_DIR}" --component all
    DEPENDS strata vellum
    USES_TERMINAL
    COMMENT "Running clang-tidy for all components"
)

if (TARGET FoliosClangTidyPlugin)
    add_custom_target(
        tidy-folios-strata
        COMMAND "${CMAKE_SOURCE_DIR}/scripts/clang_tidy.sh" --build-dir "${CMAKE_BINARY_DIR}" --component strata --folios
        DEPENDS strata FoliosClangTidyPlugin
        USES_TERMINAL
        COMMENT "Running foliOS clang-tidy checks for strata"
    )

    add_custom_target(
        tidy-folios-vellum
        COMMAND "${CMAKE_SOURCE_DIR}/scripts/clang_tidy.sh" --build-dir "${CMAKE_BINARY_DIR}" --component vellum --folios
        DEPENDS vellum FoliosClangTidyPlugin
        USES_TERMINAL
        COMMENT "Running foliOS clang-tidy checks for vellum"
    )

    add_custom_target(
        tidy-folios-all
        COMMAND "${CMAKE_SOURCE_DIR}/scripts/clang_tidy.sh" --build-dir "${CMAKE_BINARY_DIR}" --component all --folios
        DEPENDS strata vellum FoliosClangTidyPlugin
        USES_TERMINAL
        COMMENT "Running foliOS clang-tidy checks for all components"
    )
endif()
