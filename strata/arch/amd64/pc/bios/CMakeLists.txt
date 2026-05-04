cmake_minimum_required(VERSION 3.13)

if (CONFIG_ENABLE_ACPI)
    add_subdirectory(uacpi)
    
    target_compile_options(uacpi.lib PUBLIC $<$<COMPILE_LANGUAGE:C>:-mcmodel=kernel>)
    target_link_options(uacpi.lib PUBLIC -mcmodel=kernel)
endif ()

target_include_directories(strata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
