cmake_minimum_required(VERSION 3.13)

add_subdirectory(modules)

target_sources(strata PRIVATE addnode.c init.c resolve.c)
