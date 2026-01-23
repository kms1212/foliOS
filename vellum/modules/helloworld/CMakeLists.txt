cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)

set(MODULE_NAME helloworld)

# Compiler Tests

# Target Configurations
add_library(vellum_module_${MODULE_NAME} OBJECT main.c)
add_custom_target(vellum_module_${MODULE_NAME}_mod ALL
    COMMAND
        ${CMAKE_LD}
            -shared -fno-plt -z now -Bsymbolic
            --emit-relocs --export-dynamic --allow-shlib-undefined
            $<TARGET_OBJECTS:vellum_module_${MODULE_NAME}>
            -o ${MODULE_NAME}.elf
    COMMAND
        ${CMAKE_OBJCOPY}
            --add-section .note.vellum=${CMAKE_CURRENT_SOURCE_DIR}/module_info.bin
            --set-section-flags .note.vellum=contents,noload,readonly
            ${MODULE_NAME}.elf ${MODULE_NAME}.mod
    COMMAND_EXPAND_LISTS
    DEPENDS vellum_module_${MODULE_NAME}
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Linking target vellum_module_${MODULE_NAME}"
    VERBATIM
)

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
    >
)

target_include_directories(vellum_module_${MODULE_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(vellum_module_${MODULE_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(vellum_module_${MODULE_NAME} PRIVATE "${CMAKE_BINARY_DIR}")

get_target_property(vellum_INCLUDES vellum INCLUDE_DIRECTORIES)
target_include_directories(vellum_module_${MODULE_NAME} PRIVATE ${vellum_INCLUDES})
