cmake_minimum_required(VERSION 3.13)

add_subdirectory(krt)
add_subdirectory(syscall)
add_subdirectory("${TARGET_PLATFORM}")

set_source_files_properties(
    krt.S
    PROPERTIES OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/krt/krt.bin"
)

target_sources(strata PUBLIC apic.c cpufeatures.c setjmp.S)
target_sources(strata PRIVATE syscall/call.c krt.S)
add_dependencies(strata krt_cvt)

target_include_directories(strata PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/krt")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(strata PUBLIC 
    $<$<COMPILE_LANGUAGE:C>:
        -mcmodel=kernel
        -mno-sse -mno-sse2 -mno-mmx -mno-80387 -mno-avx
        -mgeneral-regs-only
        -fno-builtin
    >
)
target_link_options(strata PUBLIC -mcmodel=kernel)

if (CONFIG_ENABLE_ACPI)
    target_compile_options(uacpi.lib PUBLIC $<$<COMPILE_LANGUAGE:C>:-mcmodel=kernel>)
    target_link_options(uacpi.lib PUBLIC -mcmodel=kernel)
endif ()
