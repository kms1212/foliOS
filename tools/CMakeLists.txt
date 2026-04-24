cmake_minimum_required(VERSION 3.13)

add_subdirectory(mkfs.folifs)
add_subdirectory(folifsimg)

ExternalProject_Add(
    sidlc
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sidlc"
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/sidlc"
    BUILD_ALWAYS 1
    INSTALL_COMMAND ""
)
