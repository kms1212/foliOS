cmake_minimum_required(VERSION 3.13)

target_sources(
    strata PRIVATE addnode.c init.c interface.c iterate.c path.c refnode.c removenode.c resolve.c
)
