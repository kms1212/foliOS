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


FetchContent_Declare(openlibm
    URL "https://github.com/JuliaMath/openlibm/archive/refs/tags/v0.8.7.tar.gz"
    URL_HASH MD5=46c5a80dc8a4375a98430d74ea13d412
    DOWNLOAD_EXTRACT_TIMESTAMP false
)
set(CMAKE_PROJECT_VERSION "0.8.0")
include_directories(openlibm PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
include_directories(openlibm PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/liballoc/include")
FetchContent_MakeAvailable(openlibm)

target_include_directories(vellum PRIVATE
    "${CMAKE_BINARY_DIR}/_deps/openlibm-src/include"
)

add_library(openlibm.lib STATIC IMPORTED GLOBAL)
add_dependencies(openlibm.lib PRIVATE openlibm)
set_target_properties(openlibm.lib PROPERTIES IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/_deps/openlibm-build/libopenlibm.a")


FetchContent_Declare(zlib
    URL "https://zlib.net/zlib-1.3.1.tar.gz"
    URL_HASH SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
    DOWNLOAD_EXTRACT_TIMESTAMP false
)
include_directories(zlibstatic PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../include")
include_directories(zlibstatic PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/liballoc/include")
set(ZLIB_BUILD_EXAMPLES OFF)
FetchContent_MakeAvailable(zlib)

target_include_directories(vellum PRIVATE
    "${CMAKE_BINARY_DIR}/_deps/zlib-src"
    "${CMAKE_BINARY_DIR}/_deps/zlib-build"
)

add_library(zlib.lib STATIC IMPORTED GLOBAL)
add_dependencies(zlib.lib PRIVATE zlibstatic)
set_target_properties(zlib.lib PROPERTIES IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/_deps/zlib-build/libzlibstatic.a")


FetchContent_Declare(uacpi
    URL "https://github.com/uACPI/uACPI/archive/refs/tags/3.1.0.tar.gz"
    URL_HASH MD5=5272fab5df149aed884d86b6f77b616f
    DOWNLOAD_EXTRACT_TIMESTAMP false
)
FetchContent_MakeAvailable(uacpi)

include("${CMAKE_BINARY_DIR}/_deps/uacpi-src/uacpi.cmake")
target_include_directories(vellum PRIVATE ${UACPI_INCLUDES})

add_library(uacpi.lib STATIC)

target_compile_features(uacpi.lib PUBLIC c_std_11)
target_link_libraries(uacpi.lib gcc)
target_compile_definitions(uacpi.lib PRIVATE UACPI_PHYS_ADDR_IS_32BITS=1 UACPI_FORMATTED_LOGGING=1)

target_include_directories(uacpi.lib PRIVATE ${UACPI_INCLUDES})
target_sources(uacpi.lib PRIVATE ${UACPI_SOURCES})

target_compile_definitions(vellum PRIVATE UACPI_PHYS_ADDR_IS_32BITS=1 UACPI_FORMATTED_LOGGING=1)
