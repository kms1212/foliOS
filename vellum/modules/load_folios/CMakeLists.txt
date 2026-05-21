cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)

set(MODULE_NAME load_folios)

# Compiler Tests

# Target Configurations
add_library(vellum_module_${MODULE_NAME} OBJECT
    bootinfo.c
    handoff.c
    kernel.c
    main.c
    ramdisk.c
)
vellum_add_module_artifact(${MODULE_NAME})

# Compile Options
target_compile_features(vellum_module_${MODULE_NAME} PUBLIC c_std_11)
target_compile_options(vellum_module_${MODULE_NAME} PUBLIC
    $<$<COMPILE_LANGUAGE:C,ASM>:
        -fpic
        -fno-plt
        -Werror
        -Wall
        -Wunused-result
        -Wno-unused-function
        -mno-sse
        -mno-sse2
        -mno-mmx
    >
    $<$<COMPILE_LANGUAGE:C>:
        -mno-80387
        -mno-avx
        -mgeneral-regs-only
    >
)

target_include_directories(vellum_module_${MODULE_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(vellum_module_${MODULE_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(vellum_module_${MODULE_NAME} PRIVATE "${CMAKE_BINARY_DIR}")
target_include_directories(vellum_module_${MODULE_NAME} SYSTEM PRIVATE "${ROOT_SOURCE_DIR}/common/include")

get_target_property(vellum_INCLUDES vellum INCLUDE_DIRECTORIES)
target_include_directories(vellum_module_${MODULE_NAME} SYSTEM PRIVATE ${vellum_INCLUDES})
