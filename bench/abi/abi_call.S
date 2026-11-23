/* abi_call.S - test thunk (Linux x86-64 only): calls fn(a1..a4) with either the
 * System V or the Microsoft x64 convention, with canary values in every
 * callee-saved GPR, and reports the registers seen after the call.
 *   uint64 abi_call4(void *fn, uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 out[10], int ms)
 * out: [0]=rax [1]=rbx [2]=rbp [3..6]=r12..r15 [7]=rdi [8]=rsi [9]=rsp delta */
        .intel_syntax noprefix
        .text
        .globl  abi_call4
        .type   abi_call4, @function
abi_call4:
        push    rbx
        push    rbp
        push    r12
        push    r13
        push    r14
        push    r15
        mov     r10, qword ptr [rsp + 56]          /* 7th arg: ms flag */
        mov     qword ptr [rip + g_fn], rdi
        mov     qword ptr [rip + g_out], r9
        mov     qword ptr [rip + g_ms], r10
        test    r10, r10
        jz      1f
        mov     r9, r8                             /* MS: rcx rdx r8 r9 */
        mov     r8, rcx
        mov     rcx, rsi
        movabs  rdi, 0x7777777777777777
        movabs  rsi, 0x8888888888888888
        jmp     2f
1:      mov     rdi, rsi                           /* SysV: rdi rsi rdx rcx */
        mov     rsi, rdx
        mov     rdx, rcx
        mov     rcx, r8
2:      movabs  rbx, 0x1111111111111111
        movabs  rbp, 0x2222222222222222
        movabs  r12, 0x3333333333333333
        movabs  r13, 0x4444444444444444
        movabs  r14, 0x5555555555555555
        movabs  r15, 0x6666666666666666
        sub     rsp, 40                            /* 32-byte shadow space + alignment */
        mov     qword ptr [rip + g_rsp], rsp
        call    qword ptr [rip + g_fn]
        mov     r11, rsp
        sub     r11, qword ptr [rip + g_rsp]
        add     rsp, 40
        mov     r10, qword ptr [rip + g_out]
        mov     qword ptr [r10], rax
        mov     qword ptr [r10 + 8], rbx
        mov     qword ptr [r10 + 16], rbp
        mov     qword ptr [r10 + 24], r12
        mov     qword ptr [r10 + 32], r13
        mov     qword ptr [r10 + 40], r14
        mov     qword ptr [r10 + 48], r15
        mov     qword ptr [r10 + 56], rdi
        mov     qword ptr [r10 + 64], rsi
        mov     qword ptr [r10 + 72], r11
        pop     r15
        pop     r14
        pop     r13
        pop     r12
        pop     rbp
        pop     rbx
        ret
        .bss
g_fn:   .zero 8
g_out:  .zero 8
g_ms:   .zero 8
g_rsp:  .zero 8
        .section .note.GNU-stack,"",@progbits
