cmake_minimum_required(VERSION 3.13)

include(FetchContent)

add_subdirectory(jsmn)

if (CONFIG_ENABLE_ACPI)
    FetchContent_Declare(uacpi
        URL "https://github.com/uACPI/uACPI/archive/refs/tags/3.1.0.tar.gz"
        URL_HASH MD5=5272fab5df149aed884d86b6f77b616f
        DOWNLOAD_EXTRACT_TIMESTAMP false
    )
    FetchContent_MakeAvailable(uacpi)

    include("${uacpi_SOURCE_DIR}/uacpi.cmake")
    target_include_directories(strata SYSTEM PRIVATE ${UACPI_INCLUDES})

    add_library(uacpi.lib STATIC)

    target_compile_features(uacpi.lib PUBLIC c_std_11)
    target_link_libraries(uacpi.lib gcc)
    target_compile_definitions(uacpi.lib PRIVATE UACPI_FORMATTED_LOGGING=1 UACPI_NATIVE_ALLOC_ZEROED=1) # TODO: revert to UACPI_BAREBONES_MODE=1

    set(UACPI_INCLUDES ${UACPI_INCLUDES} PARENT_SCOPE) # TODO: remove this line

    target_include_directories(uacpi.lib PRIVATE ${UACPI_INCLUDES})
    target_sources(uacpi.lib PRIVATE ${UACPI_SOURCES})

    set_target_properties(uacpi.lib PROPERTIES C_CLANG_TIDY "")

    target_compile_definitions(strata PRIVATE UACPI_FORMATTED_LOGGING=1 UACPI_BAREBONES_MODE=1)
    target_link_libraries(strata uacpi.lib)

    add_subdirectory(uacpi)
endif ()

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
