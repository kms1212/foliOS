cmake_minimum_required(VERSION 3.13)

target_sources(vellum PRIVATE getopt.c)
target_include_directories(vellum PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
