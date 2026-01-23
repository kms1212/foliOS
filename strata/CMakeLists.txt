cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)
set(CMAKE_TOOLCHAIN_FILE "${ROOT_SOURCE_DIR}/cmake/${TARGET_ARCH}/Toolchain.cmake")

include(ExternalProject)

project("strata")

# Language configurations
enable_language(C ASM)

# Compiler Tests

# config.h
configure_file("config.h.in" "config.h")

# Dependencies
find_package(Python3 COMPONENTS Interpreter)
find_program(SPARSE_EXECUTABLE sparse)
set(SPARSE_WRAPPER "${ROOT_SOURCE_DIR}/scripts/sparse_wrapper.sh")

if (SPARSE_EXECUTABLE)
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} -print-file-name=include
        OUTPUT_VARIABLE CROSS_INC
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(SPARSE_FLAGS -Wsparse-all -Wsparse-error -D__CHECKER__ -isystem ${CROSS_INC} -D__GCC_ATOMIC_TEST_AND_SET_TRUEVAL=1 -D__STDC_VERSION__=201103L)
    set(SPARSE_COMMAND ${SPARSE_WRAPPER} ${SPARSE_EXECUTABLE} ${SPARSE_FLAGS} --)
    set(CMAKE_C_COMPILER_LAUNCHER ${SPARSE_COMMAND})
endif()

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
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(strata PRIVATE "${CMAKE_BINARY_DIR}")
target_include_directories(strata PRIVATE "${ROOT_SOURCE_DIR}/common/include")

target_compile_definitions(strata PRIVATE CRC32_FAST=1)

# Additional Targets
add_custom_target(strata_symbol_table ALL
    COMMAND ${CMAKE_NM} "$<TARGET_FILE:strata>" > strata.map
    DEPENDS strata
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Creating symbol table"
)

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
