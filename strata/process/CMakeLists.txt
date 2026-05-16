cmake_minimum_required(VERSION 3.13)

target_sources(
    strata
    PRIVATE
        dispatch.c
        gnt.c
        iterate.c
        node.c
        process.c
        process_if.c
        resolve.c
        stdio_if.c
        thread_if.c
)
