cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)
set(CMAKE_TOOLCHAIN_FILE "${ROOT_SOURCE_DIR}/cmake/${TARGET_ARCH}/Toolchain.cmake")

include(ExternalProject)

project("strata")

# Project configurations
option(BUILD_TESTING "Build tests" OFF)

# Language configurations
enable_language(C ASM)

# Compiler Tests

# config.h
configure_file("config.h.in" "config.h")

# Dependencies
find_package(Python3 COMPONENTS Interpreter)

# Target Configurations
add_executable(strata)
set_target_properties(strata PROPERTIES SUFFIX ".elf")

# Compile Options
target_compile_features(strata PUBLIC c_std_11)
target_compile_options(strata PUBLIC 
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
target_link_libraries(strata gcc)

target_include_directories(strata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(strata SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(strata PRIVATE "${CMAKE_BINARY_DIR}")
target_include_directories(strata PRIVATE "${ROOT_SOURCE_DIR}/common/include")

target_compile_definitions(strata PRIVATE CRC32_FAST=1)

# Subdirectories
add_subdirectory(lib)
add_subdirectory(arch)
add_subdirectory(init)
add_subdirectory(interrupt)
add_subdirectory(log)
add_subdirectory(mm)
add_subdirectory(mutex)
add_subdirectory(stdc)
add_subdirectory(thread)
add_subdirectory(utils)

# Additional Targets
if (BUILD_TESTING)
    ExternalProject_Add(tests
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/tests"
        BINARY_DIR "${CMAKE_BINARY_DIR}/tests"
        CMAKE_ARGS
            -DTARGET_ARCH=${TARGET_ARCH}
            -DTARGET_PLATFORM=${TARGET_PLATFORM}
            -DTARGET_FIRMWARE=${TARGET_FIRMWARE}
            -DTEST_TARGET_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DTEST_TARGET_BINARY_DIR=${CMAKE_BINARY_DIR}
            -DROOT_SOURCE_DIR=${ROOT_SOURCE_DIR}
            -DROOT_BINARY_DIR=${ROOT_BINARY_DIR}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_EXPORT_COMPILE_COMMANDS=${CMAKE_EXPORT_COMPILE_COMMANDS}
        BUILD_ALWAYS 1
        INSTALL_COMMAND ""
    )
endif()
