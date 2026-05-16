cmake_minimum_required(VERSION 3.13)

add_executable(krt entry.c syscall.S)
set_target_properties(krt PROPERTIES SUFFIX ".elf")
target_compile_features(krt PUBLIC c_std_11)
target_compile_options(krt PUBLIC 
    $<$<COMPILE_LANGUAGE:ASM,C>:
        -Werror
        -Wall
        -Wno-unused-function
        -pedantic
        -pedantic-errors
    >
    $<$<COMPILE_LANGUAGE:C>:
        -mno-sse
        -mno-sse2
        -mno-mmx
        -mno-80387
        -mno-avx
        -mgeneral-regs-only
    >
)
target_link_options(krt PUBLIC -T "${CMAKE_CURRENT_SOURCE_DIR}/ldscript_krt.lds")
set_target_properties(krt PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ldscript_krt.lds")

target_include_directories(krt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../${TARGET_PLATFORM}/include")
target_include_directories(krt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(krt SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_include_directories(krt PRIVATE "${ROOT_SOURCE_DIR}/common/include")

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/krt.bin"
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:krt>
            "${CMAKE_CURRENT_BINARY_DIR}/krt.bin"
    DEPENDS krt
    COMMENT "Converting krt binary"
    VERBATIM
)

add_custom_target(krt_cvt
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/krt.bin"
)
