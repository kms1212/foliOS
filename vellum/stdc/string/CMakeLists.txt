cmake_minimum_required(VERSION 3.13)

target_sources(vellum PRIVATE __memcpy_chk.c __memmove_chk.c __mempcpy_chk.c __memset_chk.c __stpcpy_chk.c __strcat_chk.c __strcpy_chk.c __strncat_chk.c __strncpy_chk.c)
target_sources(vellum PRIVATE memcmp.c memcpy.c memmove.c mempcpy.c memset.c stpncpy.c strchr.c strcspn.c strerror.c strncat.c strncmp.c strncpy.c strnlen.c strrchr.c strtok.c)
