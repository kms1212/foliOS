cmake_minimum_required(VERSION 3.13)

set(SOURCES "assert.c" "log.c" "panic.c")

foreach(path IN LISTS SOURCES)
    file(REAL_PATH "${path}" absolute_path)
    list(APPEND COMMON_SOURCES "${absolute_path}")
endforeach()

set(COMMON_SOURCES ${COMMON_SOURCES} PARENT_SCOPE)
