cmake_minimum_required(VERSION 3.13)

include(FetchContent)

add_subdirectory(jsmn)
add_subdirectory(liballoc)
add_subdirectory(rb)


FetchContent_Declare(zstd
    URL "https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz"
    URL_HASH SHA256=eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3
    DOWNLOAD_EXTRACT_TIMESTAMP true
    SOURCE_SUBDIR build/cmake
)

set(ZSTD_BUILD_SHARED OFF)
set(ZSTD_BUILD_EXAMPLES OFF)
set(ZSTD_MULTITHREAD_SUPPORT OFF)
set(ZSTD_BUILD_COMPRESSION OFF)
set(ZSTD_BUILD_DICTBUILDER OFF)
set(ZSTD_BUILD_PROGRAMS OFF)
set(ZSTD_BUILD_CONTRIB OFF)
set(ZSTD_BUILD_TESTS OFF)
FetchContent_MakeAvailable(zstd)

target_include_directories(libzstd_static PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(libzstd_static PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/liballoc/include")
set_target_properties(libzstd_static PROPERTIES C_CLANG_TIDY "")
target_compile_definitions(libzstd_static PRIVATE "ZSTD_NO_TRACE=1")

target_link_libraries(strata libzstd_static)
target_include_directories(strata SYSTEM PRIVATE "${CMAKE_BINARY_DIR}/_deps/zstd-src")
target_include_directories(strata SYSTEM PRIVATE "${CMAKE_BINARY_DIR}/_deps/zstd-build")
