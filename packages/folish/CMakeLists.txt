cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)

project(folish)

# Compiler Tests

# Dependencies

# Target Configurations
add_executable(folish)
set_target_properties(folish PROPERTIES SUFFIX "")

# Compile Options
target_compile_features(folish PUBLIC c_std_11)
target_compile_options(folish PUBLIC 
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
target_sources(folish PRIVATE src/main.c)

target_include_directories(folish PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(folish PRIVATE "${CMAKE_BINARY_DIR}")

# Additional Targets
add_custom_target(folish_pkg ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/pkg"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/pkg/executables"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/meta.json" "${CMAKE_BINARY_DIR}/pkg"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/folish" "${CMAKE_BINARY_DIR}/pkg/executables"
    COMMAND ${CMAKE_COMMAND} -E tar "cz" "${CMAKE_BINARY_DIR}/folish.pkg" "${CMAKE_BINARY_DIR}/pkg"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Creating package archive"
)

# Subdirectories
