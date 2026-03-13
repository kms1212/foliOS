cmake_minimum_required(VERSION 3.13)

include(FetchContent)

add_subdirectory(getopt)
add_subdirectory(liballoc)
add_subdirectory(libatomic)

if ("${TARGET_FIRMWARE}" STREQUAL "uefi")
    ExternalProject_Add(gnu-efi
        URL "https://github.com/ncroxon/gnu-efi/archive/refs/tags/4.0.2.tar.gz"
        URL_HASH MD5=5d1f5148c54564e60ea660371b85f4bd
        DOWNLOAD_EXTRACT_TIMESTAMP false
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gnu-efi"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ${CMAKE_COMMAND} -E env CROSS_COMPILE=${TOOLCHAIN_PREFIX} make -C <SOURCE_DIR>
        INSTALL_COMMAND ""
    )
endif()


FetchContent_Declare(zlib
    URL "https://zlib.net/fossils/zlib-1.3.1.tar.gz"
    URL_HASH SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
    DOWNLOAD_EXTRACT_TIMESTAMP false
)

set(ZLIB_BUILD_EXAMPLES OFF)
FetchContent_MakeAvailable(zlib)

set_target_properties(zlib PROPERTIES EXCLUDE_FROM_ALL TRUE EXCLUDE_FROM_DEFAULT_BUILD TRUE)

add_library(zlib.lib STATIC IMPORTED GLOBAL)
add_dependencies(zlib.lib PRIVATE zlibstatic)
set_target_properties(zlib.lib PROPERTIES IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/_deps/zlib-build/libzlibstatic.a")

target_include_directories(zlibstatic PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_include_directories(zlibstatic PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/liballoc/include")
set_target_properties(zlibstatic PROPERTIES C_CLANG_TIDY "")

target_link_libraries(vellum zlib.lib)
target_include_directories(vellum SYSTEM PRIVATE "${CMAKE_BINARY_DIR}/_deps/zlib-src")
target_include_directories(vellum SYSTEM PRIVATE "${CMAKE_BINARY_DIR}/_deps/zlib-build")


FetchContent_Declare(uacpi
    URL "https://github.com/uACPI/uACPI/archive/refs/tags/3.1.0.tar.gz"
    URL_HASH MD5=5272fab5df149aed884d86b6f77b616f
    DOWNLOAD_EXTRACT_TIMESTAMP false
)
FetchContent_MakeAvailable(uacpi)

include("${CMAKE_BINARY_DIR}/_deps/uacpi-src/uacpi.cmake")
target_include_directories(vellum SYSTEM PRIVATE ${UACPI_INCLUDES})

add_library(uacpi.lib STATIC)

target_compile_features(uacpi.lib PUBLIC c_std_11)
target_link_libraries(uacpi.lib gcc)
target_compile_definitions(uacpi.lib PRIVATE UACPI_PHYS_ADDR_IS_32BITS=1 UACPI_FORMATTED_LOGGING=1 UACPI_BAREBONES_MODE=1)

target_include_directories(uacpi.lib PRIVATE ${UACPI_INCLUDES})
target_sources(uacpi.lib PRIVATE ${UACPI_SOURCES})

set_target_properties(uacpi.lib PROPERTIES C_CLANG_TIDY "")

target_compile_definitions(vellum PRIVATE UACPI_PHYS_ADDR_IS_32BITS=1 UACPI_FORMATTED_LOGGING=1 UACPI_BAREBONES_MODE=1)
target_link_libraries(vellum uacpi.lib)
