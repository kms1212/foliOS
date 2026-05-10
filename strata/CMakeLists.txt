cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)
if(POLICY CMP0118)
    cmake_policy(SET CMP0118 NEW)
endif()

include(ExternalProject)

project("strata" LANGUAGES C ASM)

# SIDLC configurations
get_filename_component(SIDLC_BASE_DIR "${SIDLC_EXECUTABLE}" DIRECTORY)
file(GLOB VERSION_DIRS RELATIVE "${SIDLC_BASE_DIR}/../share" "${SIDLC_BASE_DIR}/../share/cmake-*")
set(DYNAMIC_SUFFIXES)
foreach(V_DIR ${VERSION_DIRS})
    list(APPEND DYNAMIC_SUFFIXES "${V_DIR}/Modules")
endforeach()
find_path(SIDLC_CMAKE_MODULE_PATH UseSIDLC.cmake HINTS "${SIDLC_BASE_DIR}/../share" PATH_SUFFIXES ${DYNAMIC_SUFFIXES} REQUIRED)
include("${SIDLC_CMAKE_MODULE_PATH}/UseSIDLC.cmake")

# Project configurations
option(BUILD_TESTING "Build tests" OFF)

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

# SIDL Files
sidl_generate_c(
    SERVER
    HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/sidl"
    SRCS_VAR SIDL_SRCS
    HDRS_VAR SIDL_HDRS
    FILES
        "${CMAKE_SOURCE_DIR}/sidl/process.sidl"
        "${CMAKE_SOURCE_DIR}/sidl/thread.sidl"
)

target_sources(strata PRIVATE ${SIDL_SRCS} ${SIDL_HDRS})
set_source_files_properties(${SIDL_SRCS} ${SIDL_HDRS} PROPERTIES GENERATED TRUE)
add_custom_target(sidl_gen DEPENDS ${SIDL_SRCS} ${SIDL_HDRS})
add_dependencies(strata sidl_gen)
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/sidl")

# Subdirectories
add_subdirectory(lib)
add_subdirectory(arch)
add_subdirectory(elf)
add_subdirectory(gnt)
add_subdirectory(handle)
add_subdirectory(init)
add_subdirectory(interrupt)
add_subdirectory(log)
add_subdirectory(mm)
add_subdirectory(module)
add_subdirectory(mutex)
add_subdirectory(process)
add_subdirectory(scheduler)
add_subdirectory(spinlock)
add_subdirectory(stdc)
add_subdirectory(syscall)
add_subdirectory(thread)
add_subdirectory(utils)
add_subdirectory(temp)

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
