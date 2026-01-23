cmake_minimum_required(VERSION 3.13)

add_subdirectory(folifs)
add_subdirectory(fat)
add_subdirectory(iso9660)

target_sources(vellum PRIVATE fs.c)
