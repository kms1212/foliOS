cmake_minimum_required(VERSION 3.13)

add_library(librb STATIC rb.c)
set_target_properties(librb PROPERTIES C_COMPILER_LAUNCHER "")
target_include_directories(librb PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(librb PUBLIC "${CMAKE_SOURCE_DIR}/include")

target_include_directories(strata PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(strata librb)
