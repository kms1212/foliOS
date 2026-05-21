    .org    0
    # NOTE: CMake copies this source into the build tree when krt.bin changes.

    .section .rodata
    .align  4096
    .globl  _krt_start
_krt_start:
    .incbin "krt.bin"
    .globl  _krt_end
_krt_end:
