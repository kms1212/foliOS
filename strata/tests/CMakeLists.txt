cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)

project("strata_tests")

enable_testing()

# Language configurations
enable_language(C ASM)

# Compiler Tests

# config.h
configure_file("config.h.in" "config.h")

# Dependencies
find_package(Python3 COMPONENTS Interpreter)

# Common Sources
add_subdirectory(common)

set(COMMON_SOURCES ${COMMON_SOURCES})

# Subdirectories
add_subdirectory(pmm)
