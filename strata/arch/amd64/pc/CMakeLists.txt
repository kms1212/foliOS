cmake_minimum_required(VERSION 3.13)

add_subdirectory(trampoline)

target_sources(strata PRIVATE cpulocal.c entry.S gdt.c init.c interrupt.c interrupt.S mmu.c panic.c scheduler.c syscall.c syscall.S thread.c thread.S time.c tss.c)
add_dependencies(strata trampoline_cvt)
target_link_options(strata PUBLIC -T "${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds")
set_target_properties(strata PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds")

target_include_directories(strata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/trampoline")
