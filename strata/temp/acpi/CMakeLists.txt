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

# SIDL Files
sidl_generate_c(
    SERVER
    HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/sidl"
    SRCS_VAR SIDL_SRCS
    HDRS_VAR SIDL_HDRS
    FILES
        "${ROOT_SOURCE_DIR}/sidl/acpi/table/mcfg.sidl"
)

target_sources(strata_temp_module_acpi PRIVATE ${SIDL_SRCS} ${SIDL_HDRS})
set_source_files_properties(${SIDL_SRCS} ${SIDL_HDRS} PROPERTIES GENERATED TRUE)
add_custom_target(temp_module_acpi_sidl_gen DEPENDS ${SIDL_SRCS} ${SIDL_HDRS})
add_dependencies(strata_temp_module_acpi temp_module_acpi_sidl_gen)
target_include_directories(strata_temp_module_acpi PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/sidl")

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
target_sources(strata_temp_module_acpi PRIVATE main.c ioapic.c acpi_table_mcfg_if.c)

# Subdirectories
add_subdirectory(arch)
add_subdirectory(uacpi)

# Kernel Configurations
target_link_libraries(strata strata_temp_module_acpi uacpi.lib)
