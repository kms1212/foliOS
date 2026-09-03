cmake_minimum_required(VERSION 3.13)

find_package(LLVM CONFIG
    PATHS
        "/opt/homebrew/opt/llvm/lib/cmake/llvm"
        "/usr/local/opt/llvm/lib/cmake/llvm"
)
find_package(Clang CONFIG
    PATHS
        "/opt/homebrew/opt/llvm/lib/cmake/clang"
        "/usr/local/opt/llvm/lib/cmake/clang"
)

if (NOT LLVM_FOUND OR NOT Clang_FOUND)
    message(STATUS "LLVM/Clang CMake packages not found; skipping clang-tidy-folios")
    return()
endif()

add_library(FoliosClangTidyPlugin MODULE
    ApiAnnotationsCheck.cpp
    ApiNullabilityCheck.cpp
    DistinctTypedefCheck.cpp
    NullizedParamCheck.cpp
    StatusMustCheckCheck.cpp
    FoliosTidyModule.cpp
)

target_compile_features(FoliosClangTidyPlugin PRIVATE cxx_std_17)
target_include_directories(FoliosClangTidyPlugin PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)
target_compile_definitions(FoliosClangTidyPlugin PRIVATE ${LLVM_DEFINITIONS})

target_link_libraries(FoliosClangTidyPlugin PRIVATE
    clangAST
    clangASTMatchers
    clangBasic
    clangLex
    clangTidy
    clangTidyUtils
    clangTooling
    LLVM
)

set_target_properties(FoliosClangTidyPlugin PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)
