cmake_minimum_required(VERSION 3.13)

include(FetchContent)

add_subdirectory(liballoc)
add_subdirectory(rb)

FetchContent_Declare(zlib
    URL "https://zlib.net/zlib-1.3.1.tar.gz"
    URL_HASH SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
    DOWNLOAD_EXTRACT_TIMESTAMP false
)

include_directories(zlibstatic PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
include_directories(zlibstatic PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/liballoc/include")

set(ZLIB_BUILD_EXAMPLES OFF)
FetchContent_MakeAvailable(zlib)

set_target_properties(zlib PROPERTIES EXCLUDE_FROM_ALL TRUE EXCLUDE_FROM_DEFAULT_BUILD TRUE)

set_target_properties(zlibstatic PROPERTIES C_CLANG_TIDY "")

add_library(zlib.lib STATIC IMPORTED GLOBAL)
add_dependencies(zlib.lib PRIVATE zlibstatic)
set_target_properties(zlib.lib PROPERTIES IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/_deps/zlib-build/libzlibstatic.a")

target_link_libraries(strata zlib.lib)
target_compile_options(strata PRIVATE "-isystem ${CMAKE_BINARY_DIR}/_deps/zlib-src")
target_compile_options(strata PRIVATE "-isystem ${CMAKE_BINARY_DIR}/_deps/zlib-build")
