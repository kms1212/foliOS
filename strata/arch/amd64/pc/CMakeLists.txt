cmake_minimum_required(VERSION 3.13)

add_subdirectory(trampoline)

strata_target_dependent_source(
    strata
    TRAMPOLINE_INCBIN_ASM
    "${CMAKE_CURRENT_BINARY_DIR}/trampoline.incbin.S"
    "${CMAKE_CURRENT_SOURCE_DIR}/trampoline.S"
    "${CMAKE_CURRENT_BINARY_DIR}/trampoline/trampoline.bin"
)

target_sources(strata PRIVATE cpulocal.c entry.S gdt.c hpet.c init.c interrupt.c interrupt.S mm.c panic.c pic.c pit.c scheduler.c thread.c thread.S time.c tss.c "${TRAMPOLINE_INCBIN_ASM}")
add_dependencies(strata trampoline_cvt)

strata_target_linker_script(strata "${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds")

target_include_directories(strata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/trampoline")
