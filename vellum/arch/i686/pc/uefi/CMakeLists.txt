cmake_minimum_required(VERSION 3.13)

set(BUILD_CDIMAGE TRUE CACHE BOOL "Build CD image")

set(I686_EXCLUDE_CPU_MODE TRUE PARENT_SCOPE)

target_include_directories(vellum PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(vellum PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(vellum PRIVATE "${CMAKE_BINARY_DIR}")
target_include_directories(vellum PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/gnu-efi/inc")
target_include_directories(vellum PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/gnu-efi/inc/ia32")

add_dependencies(vellum gnu-efi)

set(CMAKE_LINKER ${CMAKE_LD})

target_include_directories(vellum PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_options(vellum PUBLIC
    $<$<COMPILE_LANGUAGE:C,ASM>:
        -fpic
        -fno-stack-protector
        -fno-stack-check
        -fshort-wchar
        -mno-red-zone
        -maccumulate-outgoing-args
        -DEFI_FUNCTION_WRAPPER
    >
)
target_sources(vellum PRIVATE init.c)
target_link_options(vellum PUBLIC
    -T "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/gnu-efi/gnuefi/elf_ia32_efi.lds"
    -Wl,-shared
    -Wl,-Bsymbolic
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/gnu-efi/ia32/gnuefi/crt0-efi-ia32.o"
)
set_target_properties(vellum PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/gnu-efi/gnuefi/elf_ia32_efi.lds")
target_link_directories(vellum PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/gnu-efi/ia32/lib" "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/gnu-efi/ia32/gnuefi")
target_link_libraries(vellum gnuefi efi)

add_custom_target(vellum_efibin ALL
    COMMAND objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc -j .areloc --target efi-app-ia32 --subsystem=10 "${CMAKE_CURRENT_BINARY_DIR}/../../../../vellum.elf" vellum.efi
    DEPENDS vellum
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Creating vellum EFI binary")
