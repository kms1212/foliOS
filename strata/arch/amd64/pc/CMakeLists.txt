cmake_minimum_required(VERSION 3.13)

add_subdirectory(trampoline)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/userexec.S"
    "${CMAKE_CURRENT_BINARY_DIR}/userexec.incbin.S"
    COPYONLY
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/userexec.incbin.S.stamp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/userexec.S"
            "${CMAKE_CURRENT_BINARY_DIR}/userexec.incbin.S"
    COMMAND ${CMAKE_COMMAND} -E touch
            "${CMAKE_CURRENT_BINARY_DIR}/userexec.incbin.S.stamp"
    DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/userexec.S"
            "${CMAKE_CURRENT_SOURCE_DIR}/main.app"
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/userexec.incbin.S"
    VERBATIM
)
set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/userexec.incbin.S"
    PROPERTIES GENERATED TRUE
)
add_custom_target(userexec_cvt
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/userexec.incbin.S.stamp"
)

target_sources(strata PRIVATE cpulocal.c entry.S gdt.c init.c interrupt.c interrupt.S mm.c panic.c scheduler.c thread.c thread.S time.c trampoline.S tss.c "${CMAKE_CURRENT_BINARY_DIR}/userexec.incbin.S")
add_dependencies(strata trampoline_cvt)
add_dependencies(strata userexec_cvt)

target_link_options(strata PUBLIC -T "${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds")
set_target_properties(strata PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds")

target_include_directories(strata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/trampoline")
