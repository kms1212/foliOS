cmake_minimum_required(VERSION 3.13)

add_subdirectory("${TARGET_PLATFORM}")

target_sources(strata PUBLIC cpufeatures.c setjmp.S)
target_include_directories(strata PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(strata PUBLIC $<$<COMPILE_LANGUAGE:C>:-mcmodel=kernel>)
target_link_options(strata PUBLIC -mcmodel=kernel)
