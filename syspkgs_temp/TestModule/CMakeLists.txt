cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)
if(POLICY CMP0118)
    cmake_policy(SET CMP0118 NEW)
endif()

project("TestModule" LANGUAGES C ASM)

include(UseSIDLC)
include(UseSMA)

if(NOT SIDLC_INTERFACE_DIRECTORY)
    set(
        SIDLC_INTERFACE_DIRECTORY
        "${CMAKE_CURRENT_LIST_DIR}/../../strata/sidl"
        CACHE PATH
        "SIDL interface source directory"
    )
endif()

# SIDL Compilations
sidlc_compile(
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/sidl"
    SIFS_VAR SIDL_SIFS
    FILES "${SIDLC_INTERFACE_DIRECTORY}/byte_stream.sidl"
)
sidlc_generate(
    SERVER
    HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/sidl"
    SRCS_VAR SIDL_SRCS
    HDRS_VAR SIDL_HDRS
    SIFS ${SIDL_SIFS}
)

# Project configurations
option(BUILD_TESTING "Build tests" OFF)

sma_add_module(TestModule PROVIDES ${SIDL_SIFS})
target_sources(TestModule_KERNEL PRIVATE kmain.c)
target_sources(TestModule_USER PRIVATE umain.c ${SIDL_SRCS} ${SIDL_HDRS})
target_include_directories(TestModule_USER PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/sidl")
