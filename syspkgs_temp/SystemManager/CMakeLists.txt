cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)
if(POLICY CMP0118)
    cmake_policy(SET CMP0118 NEW)
endif()

project("SystemManager" LANGUAGES C ASM)

# Project configurations
option(BUILD_TESTING "Build tests" OFF)

# Target Configurations
add_executable(SystemManager)
target_sources(SystemManager PRIVATE main.c)
set_target_properties(SystemManager PROPERTIES OUTPUT_NAME "main.app" SUFFIX "")

# Compile Options
target_compile_features(SystemManager PUBLIC c_std_11)
target_compile_options(SystemManager PUBLIC
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
target_link_options(SystemManager PUBLIC -static)
