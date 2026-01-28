    .org    0

    .section .rodata
    .align  4096
    .globl  _krt_start
_krt_start:
    .incbin "krt.bin"
    .globl  _krt_end
_krt_end:
