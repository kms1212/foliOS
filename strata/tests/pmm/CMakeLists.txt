cmake_minimum_required(VERSION 3.13)

# Target Configurations
add_executable(pmm_test ${COMMON_SOURCES} "${TEST_TARGET_SOURCE_DIR}/mm/pmm.c" main.c)

# Compile Options
target_compile_features(pmm_test PUBLIC c_std_11)
target_compile_options(pmm_test PUBLIC 
    $<$<COMPILE_LANGUAGE:C>:
        -Wall
        -Wno-unused-function
    >
)
target_link_libraries(pmm_test c System)

target_include_directories(pmm_test PRIVATE "${CMAKE_BINARY_DIR}")
target_include_directories(pmm_test PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_include_directories(pmm_test PRIVATE "${ROOT_SOURCE_DIR}/common/include")
target_include_directories(pmm_test PRIVATE "${TEST_TARGET_SOURCE_DIR}/arch/${TARGET_ARCH}/include")
target_include_directories(pmm_test PRIVATE "${TEST_TARGET_SOURCE_DIR}/arch/${TARGET_ARCH}/${TARGET_PLATFORM}/include")
target_include_directories(pmm_test PRIVATE "${TEST_TARGET_SOURCE_DIR}/include")

target_compile_definitions(pmm_test PRIVATE "TESTING=1")

add_test(NAME pmm_test COMMAND pmm_test)
