cmake_minimum_required(VERSION 3.13)

add_subdirectory("${TARGET_PLATFORM}")

target_sources(strata PUBLIC cpufeatures.c setjmp.S)
target_include_directories(strata PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(strata PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(strata PUBLIC 
    $<$<COMPILE_LANGUAGE:C>:
        -mcmodel=kernel
        -mno-sse -mno-sse2 -mno-mmx -mno-80387 -mno-avx
        -mgeneral-regs-only
    >
)
target_link_options(strata PUBLIC -mcmodel=kernel)

target_compile_options(librb PUBLIC 
    $<$<COMPILE_LANGUAGE:C>:
        -mcmodel=kernel
        -mno-sse -mno-sse2 -mno-mmx -mno-80387 -mno-avx
        -mgeneral-regs-only
    >
)

target_compile_options(liballoc PUBLIC 
    $<$<COMPILE_LANGUAGE:C>:
        -mcmodel=kernel
        -mno-sse -mno-sse2 -mno-mmx -mno-80387 -mno-avx
        -mgeneral-regs-only
    >
)
