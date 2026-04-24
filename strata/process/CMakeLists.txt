cmake_minimum_required(VERSION 3.13)

target_sources(
    strata
    PRIVATE
        dispatch.c
        gnt.c
        iterate.c
        process.c
        process_if.c
        resolve.c
        thread_if.c
)
