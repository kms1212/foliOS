cmake_minimum_required(VERSION 3.13)

add_subdirectory("${TARGET_FIRMWARE}")

set(I686_EXCLUDE_CPU_MODE ${I686_EXCLUDE_CPU_MODE} PARENT_SCOPE)
