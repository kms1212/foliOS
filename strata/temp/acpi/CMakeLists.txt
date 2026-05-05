cmake_minimum_required(VERSION 3.13)

# Target Configurations
add_library(strata_temp_module_acpi STATIC)

# Compile Options
target_compile_features(strata_temp_module_acpi PUBLIC c_std_11)
target_compile_options(strata_temp_module_acpi PUBLIC 
    $<$<COMPILE_LANGUAGE:C,ASM>:
        -Werror
        -Wall
        -Wno-unused-function
        -Wno-unused-variable
        -Wno-unused-but-set-variable
        -pedantic
        -pedantic-errors
    >
)
target_link_libraries(strata_temp_module_acpi gcc)
target_compile_definitions(strata_temp_module_acpi PRIVATE UACPI_FORMATTED_LOGGING=1 UACPI_NATIVE_ALLOC_ZEROED=1)

# Include Directories
target_include_directories(strata_temp_module_acpi PRIVATE "${CMAKE_SOURCE_DIR}")
target_include_directories(strata_temp_module_acpi SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_include_directories(strata_temp_module_acpi SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}/arch/generic/include")
target_include_directories(strata_temp_module_acpi SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}/arch/${TARGET_ARCH}/include")
target_include_directories(strata_temp_module_acpi SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}/arch/${TARGET_ARCH}/${TARGET_PLATFORM}/include")
target_include_directories(strata_temp_module_acpi SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}/arch/${TARGET_ARCH}/${TARGET_PLATFORM}/${TARGET_FIRMWARE}/include")
target_include_directories(strata_temp_module_acpi SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(strata_temp_module_acpi PRIVATE "${CMAKE_BINARY_DIR}")
target_include_directories(strata_temp_module_acpi PRIVATE "${ROOT_SOURCE_DIR}/common/include")
target_include_directories(strata_temp_module_acpi SYSTEM PRIVATE ${UACPI_INCLUDES})

# Sources
target_sources(strata_temp_module_acpi PRIVATE main.c)

add_subdirectory(arch)
add_subdirectory(uacpi)

target_link_libraries(strata strata_temp_module_acpi uacpi.lib)
