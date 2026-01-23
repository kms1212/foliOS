cmake_minimum_required(VERSION 3.13)

add_subdirectory(part)

target_sources(vellum PRIVATE ata.c atapi.c  fdc_isa.c floppy.c ide_isa.c)
