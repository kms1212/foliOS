cmake_minimum_required(VERSION 3.13)

if (CONFIG_ENABLE_ACPI)
    target_compile_options(strata_temp_module_acpi PUBLIC $<$<COMPILE_LANGUAGE:C>:-mcmodel=kernel>)
    target_link_options(strata_temp_module_acpi PUBLIC -mcmodel=kernel)
endif ()

target_include_directories(strata_temp_module_acpi SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
