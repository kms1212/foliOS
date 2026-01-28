    .org    0

#include <strata/syscall.h>

    .section .text
    .code64
    .globl  do_syscall
do_syscall:
    push    %rbp
    mov     %rsp, %rbp

    mov     %r9, %rax                   # syscall number
    mov     %rcx, %r10                  # convert user abi to syscall abi
    syscall

    leave
    ret 

    .globl do_node_call_syscall
do_node_call_syscall:
    push    %rbp
    mov     %rsp, %rbp

    mov     $SYS_NODE_CALL, %rax        # syscall number
    mov     %rcx, %r10                  # convert user abi to syscall abi
    syscall

    leave
    ret

