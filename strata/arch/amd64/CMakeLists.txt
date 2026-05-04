cmake_minimum_required(VERSION 3.13)

add_subdirectory(krt)
add_subdirectory(syscall)
add_subdirectory("${TARGET_PLATFORM}")

set(SIDLC_EXECUTABLE "${ROOT_BINARY_DIR}/tools/sidlc/sidlc")
include("${ROOT_SOURCE_DIR}/tools/sidlc/cmake/UseSIDLC.cmake")

sidl_generate_c(
    MODULE
    HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/sidl"
    SRCS_VAR ARCH_AMD64_SIDL_SRCS
    HDRS_VAR ARCH_AMD64_SIDL_HDRS
    FILES
        "${ROOT_SOURCE_DIR}/tools/sidlc/interfaces/process.sidl"
        "${ROOT_SOURCE_DIR}/tools/sidlc/interfaces/thread.sidl"
)

set_source_files_properties(
    ${ARCH_AMD64_SIDL_SRCS}
    ${ARCH_AMD64_SIDL_HDRS}
    PROPERTIES GENERATED TRUE
)

add_custom_target(
    arch_amd64_sidl_gen
    DEPENDS ${ARCH_AMD64_SIDL_SRCS} ${ARCH_AMD64_SIDL_HDRS}
)

set_source_files_properties(
    krt.S
    PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/krt/krt.bin"
)

target_sources(strata PUBLIC apic.c cpufeatures.c setjmp.S)
target_sources(
    strata
    PRIVATE
        syscall/call.c
        krt.S
        syscall_stdio.c
        ${ARCH_AMD64_SIDL_SRCS}
        ${ARCH_AMD64_SIDL_HDRS}
)
add_dependencies(strata krt_cvt)
add_dependencies(strata arch_amd64_sidl_gen)

target_include_directories(strata PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/krt")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/sidl")
target_compile_options(strata PUBLIC 
    $<$<COMPILE_LANGUAGE:C>:
        -mcmodel=kernel
        -mno-sse -mno-sse2 -mno-mmx -mno-80387 -mno-avx
        -mgeneral-regs-only
        -fno-builtin
    >
)
target_link_options(strata PUBLIC -mcmodel=kernel)
