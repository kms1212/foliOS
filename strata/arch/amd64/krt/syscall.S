    .org    0

#include <strata/plat/syscall_num.h>

    .section .text
    .code64
    .globl  node_open_syscall
node_open_syscall:
    push    %rbp
    mov     %rsp, %rbp

    mov     $SYS_NODE_OPEN, %rax        # syscall number
    mov     %rcx, %r10                  # convert user abi to syscall abi
    syscall

    leave
    ret

    .globl node_close_syscall
node_close_syscall:
    push    %rbp
    mov     %rsp, %rbp

    mov     $SYS_NODE_CLOSE, %rax        # syscall number
    mov     %rcx, %r10                  # convert user abi to syscall abi
    syscall

    leave
    ret

    .globl node_query_syscall
node_query_syscall:
    push    %rbp
    mov     %rsp, %rbp

    mov     $SYS_NODE_QUERY, %rax        # syscall number
    mov     %rcx, %r10                  # convert user abi to syscall abi
    syscall

    leave
    ret

    .globl node_call0_syscall
node_call0_syscall:
    .globl node_call1_syscall
node_call1_syscall:
    .globl node_call2_syscall
node_call2_syscall:
    .globl node_call3_syscall
node_call3_syscall:
    .globl node_call4_syscall
node_call4_syscall:
    push    %rbp
    mov     %rsp, %rbp

    mov     $SYS_NODE_CALL_REG, %rax    # syscall number
    mov     %rcx, %r10                  # convert user abi to syscall abi
    syscall

    leave
    ret

    .globl node_call_n_syscall
node_call_n_syscall:
    push    %rbp
    mov     %rsp, %rbp

    mov     $SYS_NODE_CALL_PTR, %rax    # syscall number
    mov     %rcx, %r10                  # convert user abi to syscall abi
    syscall

    leave
    ret

