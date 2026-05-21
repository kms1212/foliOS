cmake_minimum_required(VERSION 3.13)

add_executable(fdboot)
set_target_properties(fdboot PROPERTIES SUFFIX ".elf")
target_compile_features(fdboot PUBLIC c_std_11)
target_compile_options(fdboot PUBLIC 
    $<$<COMPILE_LANGUAGE:C,ASM>:
        -Werror
        -Wall
        -Wno-unused-function
        -pedantic
        -pedantic-errors
    >
)
target_link_options(fdboot PUBLIC -T "${CMAKE_CURRENT_SOURCE_DIR}/ldscript_vbr.lds")
set_target_properties(fdboot PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ldscript_vbr.lds")
target_sources(fdboot PRIVATE fdboot.S)

vellum_add_binary_artifact(
    fdboot_execbin
    fdboot
    "${CMAKE_CURRENT_BINARY_DIR}/fdboot.bin"
    "Creating floppy VBR"
)


add_executable(mbrboot)
set_target_properties(mbrboot PROPERTIES SUFFIX ".elf")
target_compile_features(mbrboot PUBLIC c_std_11)
target_compile_options(mbrboot PUBLIC 
    $<$<COMPILE_LANGUAGE:C,ASM>:
        -Werror
        -Wall
        -Wno-unused-function
        -pedantic
        -pedantic-errors
    >
)
target_link_options(mbrboot PUBLIC -T "${CMAKE_CURRENT_SOURCE_DIR}/ldscript_vbr.lds")
set_target_properties(mbrboot PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ldscript_vbr.lds")
target_sources(mbrboot PRIVATE mbrboot.S)

vellum_add_binary_artifact(
    mbrboot_execbin
    mbrboot
    "${CMAKE_CURRENT_BINARY_DIR}/mbrboot.bin"
    "Creating MBR boot sector"
)

add_executable(stage1)
set_target_properties(stage1 PROPERTIES SUFFIX ".elf")

target_compile_features(stage1 PUBLIC c_std_11)
target_compile_options(stage1 PUBLIC 
    $<$<COMPILE_LANGUAGE:C,ASM>:
        -Werror
        -Wall
        -Wno-unused-function
        -pedantic
        -pedantic-errors
        -fno-function-sections
        -fno-data-sections
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
target_link_libraries(stage1 gcc)

target_include_directories(stage1 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(stage1 SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(stage1 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_include_directories(stage1 SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
target_include_directories(stage1 SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include")
target_include_directories(stage1 PRIVATE "${CMAKE_BINARY_DIR}")

target_sources(stage1 PRIVATE stage1.S stage1.c ../../cpu_mode.S pic.c)
target_link_options(stage1 PUBLIC -T "${CMAKE_CURRENT_SOURCE_DIR}/ldscript_stage1.lds")
set_target_properties(stage1 PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ldscript_stage1.lds")
target_compile_options(stage1 PUBLIC -Wno-array-bounds -fno-delete-null-pointer-checks)

vellum_add_binary_artifact(
    stage1_execbin
    stage1
    "${CMAKE_CURRENT_BINARY_DIR}/stage1.bin"
    "Creating stage1 binary"
)

set(I686_EXCLUDE_CPU_MODE TRUE PARENT_SCOPE)
add_dependencies(vellum stage1)
target_link_options(vellum PUBLIC -Wl,--just-symbols=$<TARGET_FILE:stage1>)
set_target_properties(vellum PROPERTIES LINK_DEPENDS stage1.elf)
target_compile_options(vellum PUBLIC -Wno-array-bounds -fno-delete-null-pointer-checks)

target_include_directories(vellum PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(vellum SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(vellum PRIVATE "${CMAKE_BINARY_DIR}")

target_sources(vellum PRIVATE gdt.c init.c isr.c isr.S panic.c power.c start.S)
target_link_options(vellum PUBLIC -T "${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds" -Wl,-print-gc-sections)
set_target_properties(vellum PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds;${CMAKE_CURRENT_BINARY_DIR}/stage1.elf")

add_subdirectory(acpi)
add_subdirectory(bios)
add_subdirectory(device)
add_subdirectory(shell)

vellum_add_binary_artifact(
    vellum_execbin
    vellum
    "${CMAKE_CURRENT_BINARY_DIR}/vellum.bin"
    "Creating vellum executable binary"
)
