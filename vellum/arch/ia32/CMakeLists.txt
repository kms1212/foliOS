cmake_minimum_required(VERSION 3.13)

add_subdirectory("${TARGET_PLATFORM}")

if (NOT I686_EXCLUDE_CPU_MODE)
    target_sources(vellum PRIVATE cpu_mode.S)
endif()
target_sources(vellum PRIVATE setjmp.S cpufeatures.c)
target_include_directories(vellum PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(vellum PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
