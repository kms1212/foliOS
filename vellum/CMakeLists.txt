cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)

project("vellum" LANGUAGES C ASM)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/VellumArtifacts.cmake")

# Compiler Tests

# config.h
configure_file("config.h.in" "config.h")

# Dependencies
find_package(Python3 COMPONENTS Interpreter)

# Target Configurations
add_executable(vellum)
set_target_properties(vellum PROPERTIES SUFFIX ".elf")

# Compile Options
target_compile_features(vellum PUBLIC c_std_11)
target_compile_options(vellum PUBLIC 
    $<$<COMPILE_LANGUAGE:C,ASM>:
        -Werror
        -Wall
        -Wunused-result
        -Wno-unused-function
        -Wno-unused-const-variable
        -mno-sse
        -mno-sse2
        -mno-mmx
    >
    $<$<COMPILE_LANGUAGE:C>:
        -mno-80387
        -mno-avx
        -mgeneral-regs-only
    >
)
target_link_libraries(vellum gcc)

target_include_directories(vellum PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(vellum SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(vellum PRIVATE "${CMAKE_BINARY_DIR}")

target_compile_definitions(vellum PRIVATE CRC32_FAST=1)

# Additional Targets
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/vellum.map"
    COMMAND /bin/sh -c "\"${CMAKE_NM}\" \"$<TARGET_FILE:vellum>\" > \"${CMAKE_CURRENT_BINARY_DIR}/vellum.map\""
    DEPENDS vellum
    COMMENT "Creating symbol table"
    VERBATIM
)
add_custom_target(vellum_symbol_table ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/vellum.map"
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/exported_symbols.c"
    COMMAND ${Python3_EXECUTABLE}
            "${ROOT_SOURCE_DIR}/tools/mksymtab/mksymtab.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/exported_symbols.txt"
            "${CMAKE_CURRENT_BINARY_DIR}/exported_symbols.c"
    DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/exported_symbols.txt"
            "${ROOT_SOURCE_DIR}/tools/mksymtab/mksymtab.py"
    VERBATIM
)
add_custom_target(vellum_exported_symbol_table ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/exported_symbols.c"
    COMMENT "Creating exported symbol table"
)
add_dependencies(vellum vellum_exported_symbol_table)
target_sources(vellum PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/exported_symbols.c")
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/exported_symbols.c" PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/unifont.bfn"
    COMMAND ${Python3_EXECUTABLE}
            "${ROOT_SOURCE_DIR}/tools/hex2bfn/hex2bfn.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/unifont.hex"
            "${CMAKE_CURRENT_BINARY_DIR}/unifont.bfn"
    DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/unifont.hex"
            "${ROOT_SOURCE_DIR}/tools/hex2bfn/hex2bfn.py"
    COMMENT "Generating font data"
    VERBATIM
)
add_custom_target(vellum_font ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/unifont.bfn"
)

# libraries
add_subdirectory(lib)

# Subdirectories
add_subdirectory(arch)

if (NOT "${TARGET_FIRMWARE}" STREQUAL "uefi")
    add_subdirectory(core)
    add_subdirectory(debug)
    add_subdirectory(device)
    add_subdirectory(elf)
    add_subdirectory(encoding)
    add_subdirectory(filesystem)
    add_subdirectory(font)
    add_subdirectory(json)
    add_subdirectory(mm)
    add_subdirectory(module)
    add_subdirectory(stdc)
    add_subdirectory(shell)
    add_subdirectory(modules)
endif()
