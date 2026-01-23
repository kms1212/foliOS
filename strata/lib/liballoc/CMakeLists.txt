cmake_minimum_required(VERSION 3.13)

add_library(liballoc STATIC liballoc.c)
set_target_properties(liballoc PROPERTIES C_COMPILER_LAUNCHER "")
target_include_directories(liballoc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_sources(strata PRIVATE platform.c)
target_include_directories(strata PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(strata liballoc)
