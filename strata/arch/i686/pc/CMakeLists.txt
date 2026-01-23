cmake_minimum_required(VERSION 3.13)

target_sources(strata PRIVATE entry.S gdt.c init.c interrupt.c interrupt.S mmu.c panic.c scheduler.c thread.c thread.S tss.c)
target_link_options(strata PUBLIC -T "${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds")
set_target_properties(strata PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds")

target_include_directories(strata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
