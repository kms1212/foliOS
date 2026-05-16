cmake_minimum_required(VERSION 3.13)

add_subdirectory("${TARGET_FIRMWARE}")
add_subdirectory(trampoline)

if(NOT STRATA_USEREXEC_APP)
    message(FATAL_ERROR "STRATA_USEREXEC_APP must point to the user executable embedded by userexec.S")
endif()

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
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${STRATA_USEREXEC_APP}"
            "${CMAKE_CURRENT_BINARY_DIR}/main.app"
    COMMAND ${CMAKE_COMMAND} -E touch
            "${CMAKE_CURRENT_BINARY_DIR}/userexec.incbin.S.stamp"
    DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/userexec.S"
            "${STRATA_USEREXEC_APP}"
    BYPRODUCTS
            "${CMAKE_CURRENT_BINARY_DIR}/userexec.incbin.S"
            "${CMAKE_CURRENT_BINARY_DIR}/main.app"
    VERBATIM
)
set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/userexec.incbin.S"
    PROPERTIES GENERATED TRUE
)
add_custom_target(userexec_cvt
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/userexec.incbin.S.stamp"
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/trampoline.S"
    "${CMAKE_CURRENT_BINARY_DIR}/trampoline.incbin.S"
    COPYONLY
)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/trampoline.incbin.S.stamp"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/trampoline.S"
            "${CMAKE_CURRENT_BINARY_DIR}/trampoline.incbin.S"
    COMMAND ${CMAKE_COMMAND} -E touch
            "${CMAKE_CURRENT_BINARY_DIR}/trampoline.incbin.S.stamp"
    DEPENDS
            trampoline_cvt
            "${CMAKE_CURRENT_SOURCE_DIR}/trampoline.S"
            "${CMAKE_CURRENT_BINARY_DIR}/trampoline/trampoline.bin"
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/trampoline.incbin.S"
    VERBATIM
)
set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/trampoline.incbin.S"
    PROPERTIES GENERATED TRUE
)
add_custom_target(trampoline_incbin
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/trampoline.incbin.S.stamp"
)

target_sources(strata PRIVATE cpulocal.c entry.S gdt.c hpet.c init.c interrupt.c interrupt.S mm.c panic.c pic.c pit.c scheduler.c thread.c thread.S time.c tss.c "${CMAKE_CURRENT_BINARY_DIR}/trampoline.incbin.S" "${CMAKE_CURRENT_BINARY_DIR}/userexec.incbin.S")
add_dependencies(strata trampoline_cvt)
add_dependencies(strata trampoline_incbin)
add_dependencies(strata userexec_cvt)

target_link_options(strata PUBLIC -T "${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds")
set_target_properties(strata PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ldscript.lds")

target_include_directories(strata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/trampoline")
