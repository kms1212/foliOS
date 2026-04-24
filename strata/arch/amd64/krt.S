    .org    0
    # NOTE: this source depends on generated krt.bin (see CMake OBJECT_DEPENDS).

    .section .rodata
    .align  4096
    .globl  _krt_start
_krt_start:
    .incbin "krt.bin"
    .globl  _krt_end
_krt_end:
