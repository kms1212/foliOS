set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX "x86_64-strata-folios-")
find_program(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}gcc" ENV PATH)

if (CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER_TARGET x86_64-strata-folios)
    set(CMAKE_CXX_COMPILER_TARGET x86_64-strata-folios)
    set(CMAKE_ASM_COMPILER_TARGET x86_64-strata-folios)
else()
    set(TOOLCHAIN_PREFIX "x86_64-elf-")
    find_program(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}gcc" ENV PATH REQUIRED)

    set(CMAKE_C_COMPILER_TARGET x86_64-elf)
    set(CMAKE_CXX_COMPILER_TARGET x86_64-elf)
    set(CMAKE_ASM_COMPILER_TARGET x86_64-elf)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding -nostdlib -fno-stack-protector -mno-red-zone -fno-pic -Wl,-no-pie -fno-pie")

find_program(CMAKE_CXX_COMPILER "${TOOLCHAIN_PREFIX}g++" ENV PATH REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffreestanding -nostdlib -fno-stack-protector -mno-red-zone -fno-pic -Wl,-no-pie -fno-pie")

set(CMAKE_ASM_COMPILER "${CMAKE_C_COMPILER}")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -ffreestanding -nostdlib -fno-stack-protector -mno-red-zone -fno-pic -Wl,-no-pie -fno-pie")

set(_BINUTILS_LIST LD;AR;NM;OBJCOPY;OBJDUMP;RANLIB;READELF;STRIP)

foreach(TOOL_NAME IN LISTS _BINUTILS_LIST)
    string(TOLOWER ${TOOL_NAME} _TOOL_BIN_NAME)
    find_program(CMAKE_${TOOL_NAME} "${TOOLCHAIN_PREFIX}${_TOOL_BIN_NAME}" ENV PATH REQUIRED)
    unset(_TOOL_BIN_NAME)
endforeach()

unset(_BINUTILS_LIST)

find_program(SIDLC_EXECUTABLE "sidlc" ENV PATH REQUIRED)

set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--gc-sections")
