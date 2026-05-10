cmake_minimum_required(VERSION 3.13)

execute_process(
    COMMAND ${CMAKE_C_COMPILER} -m32 -print-file-name=include
    OUTPUT_VARIABLE GCC_BUILTIN_INC_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(TRAMPOLINE_LINK_EMULATION elf_i386)
set(TRAMPOLINE_LD "${CMAKE_LD}")
if (NOT TRAMPOLINE_LD)
    set(TRAMPOLINE_LD "${CMAKE_LINKER}")
endif()
if (TRAMPOLINE_LD)
    execute_process(
        COMMAND ${TRAMPOLINE_LD} -V
        OUTPUT_VARIABLE TRAMPOLINE_LD_EMULATIONS
        ERROR_VARIABLE TRAMPOLINE_LD_EMULATIONS
        RESULT_VARIABLE TRAMPOLINE_LD_RESULT
    )
    if (TRAMPOLINE_LD_RESULT EQUAL 0 AND
        TRAMPOLINE_LD_EMULATIONS MATCHES "(^|[ \r\n])elf_i386_folios([ \r\n]|$)")
        set(TRAMPOLINE_LINK_EMULATION elf_i386_folios)
    endif()
endif()

add_executable(trampoline entry.S trampoline.c)
set_target_properties(trampoline PROPERTIES SUFFIX ".elf")
set_target_properties(trampoline PROPERTIES C_CLANG_TIDY "")
target_compile_features(trampoline PUBLIC c_std_11)
target_compile_options(trampoline PUBLIC 
    $<$<COMPILE_LANGUAGE:ASM,C>:
        -Werror
        -Wall
        -Wno-unused-function
        -pedantic
        -pedantic-errors
        -m32
        -Wa,--32
        -fno-asynchronous-unwind-tables
        -fno-unwind-tables
        -nostdinc
        -isystem "${GCC_BUILTIN_INC_PATH}"
    >
)
target_link_options(trampoline PUBLIC
    -m32
    -Wl,-m,${TRAMPOLINE_LINK_EMULATION}
    -T "${CMAKE_CURRENT_SOURCE_DIR}/ldscript_trampoline.lds"
)
set_target_properties(trampoline PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ldscript_trampoline.lds")

target_include_directories(trampoline PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(trampoline PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
target_include_directories(trampoline SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_include_directories(trampoline PRIVATE "${ROOT_SOURCE_DIR}/common/include")
target_include_directories(trampoline PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

add_custom_target(trampoline_cvt ALL
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:trampoline> trampoline.bin
    DEPENDS trampoline
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/trampoline.bin"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Converting trampoline binary"
)
