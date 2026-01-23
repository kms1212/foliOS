cmake_minimum_required(VERSION 3.13)

add_executable(trampoline entry.S trampoline.c)
set_target_properties(trampoline PROPERTIES SUFFIX ".elf")
target_compile_features(trampoline PUBLIC c_std_11)
target_compile_options(trampoline PUBLIC 
    $<$<COMPILE_LANGUAGE:ASM,C>:
        -Werror
        -Wall
        -Wno-unused-function
        -pedantic
        -pedantic-errors
        -m32
    >
)
target_link_options(trampoline PUBLIC -T "${CMAKE_CURRENT_SOURCE_DIR}/ldscript_trampoline.lds")
set_target_properties(trampoline PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ldscript_trampoline.lds")

target_include_directories(trampoline PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(trampoline PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
target_include_directories(trampoline PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_include_directories(trampoline PRIVATE "${ROOT_SOURCE_DIR}/common/include")

add_custom_target(trampoline_cvt ALL
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:trampoline> trampoline.bin
    DEPENDS trampoline
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/trampoline.bin"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Converting trampoline binary"
)
