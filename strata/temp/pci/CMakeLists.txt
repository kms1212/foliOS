cmake_minimum_required(VERSION 3.13)

# Target Configurations
add_library(strata_temp_module_pci STATIC)

# Compile Options
target_compile_features(strata_temp_module_pci PUBLIC c_std_11)
target_compile_options(strata_temp_module_pci PUBLIC
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
target_link_libraries(strata_temp_module_pci gcc)
target_compile_definitions(strata_temp_module_pci PRIVATE UACPI_FORMATTED_LOGGING=1 UACPI_NATIVE_ALLOC_ZEROED=1)

# SIDL Files
sidl_generate_c(
    SERVER_CLIENT
    HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/sidl"
    SRCS_VAR SIDL_SRCS
    HDRS_VAR SIDL_HDRS
    FILES
        "${ROOT_SOURCE_DIR}/sidl/acpi/table/mcfg.sidl"
)

target_sources(strata_temp_module_pci PRIVATE ${SIDL_SRCS} ${SIDL_HDRS})
set_source_files_properties(${SIDL_SRCS} ${SIDL_HDRS} PROPERTIES GENERATED TRUE)
add_custom_target(temp_module_pci_sidl_gen DEPENDS ${SIDL_SRCS} ${SIDL_HDRS})
add_dependencies(strata_temp_module_pci temp_module_pci_sidl_gen)
target_include_directories(strata_temp_module_pci PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/sidl")

# Include Directories
target_include_directories(strata_temp_module_pci PRIVATE "${CMAKE_SOURCE_DIR}")
target_include_directories(strata_temp_module_pci SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_include_directories(strata_temp_module_pci SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}/arch/generic/include")
target_include_directories(strata_temp_module_pci SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}/arch/${TARGET_ARCH}/include")
target_include_directories(strata_temp_module_pci SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}/arch/${TARGET_ARCH}/${TARGET_PLATFORM}/include")
target_include_directories(strata_temp_module_pci SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}/arch/${TARGET_ARCH}/${TARGET_PLATFORM}/${TARGET_FIRMWARE}/include")
target_include_directories(strata_temp_module_pci SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(strata_temp_module_pci PRIVATE "${CMAKE_BINARY_DIR}")
target_include_directories(strata_temp_module_pci PRIVATE "${ROOT_SOURCE_DIR}/common/include")
target_include_directories(strata_temp_module_pci SYSTEM PRIVATE ${UACPI_INCLUDES})

# Sources
target_sources(strata_temp_module_pci PRIVATE main.c scan.c)

add_subdirectory(arch)

target_link_libraries(strata strata_temp_module_pci)
