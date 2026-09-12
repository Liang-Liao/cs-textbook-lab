/*
 * Lab 09: 浮点运算
 * Windows MSYS2 UCRT64 — Microsoft x64 ABI
 *
 * 要点: 浮点参数不走 RCX/RDX, 走 XMM0-XMM3
 *       printf 这类 variadic 函数要求 AL = 用到的 XMM 个数
 *       序言 sub $48: 32 字节 shadow space + 16 字节局部空间
 */

.section .rodata
msg_pi:    .asciz "pi * 2.0 + 1.0 = %.6f\n"
msg_fsum:  .asciz "sum(1.1,2.2,3.3,4.4,5.5) = %.2f\n"
msg_cmp:   .asciz "pi > 2.0? %s\n"
yes_str:   .asciz "yes"
no_str:    .asciz "no"

.section .data
    .align 16
val1: .double 3.14159265358979
val2: .double 2.0
val3: .double 1.0
farray:
    .double 1.1, 2.2, 3.3, 4.4, 5.5
    farray_len = 5

.section .text
.globl main

main:
    push %rbp
    mov %rsp, %rbp
    sub $48, %rsp              /* 32 shadow space + 16 局部空间 */

    /* === 练习 1: pi * 2.0 + 1.0 === */
    movsd val1(%rip), %xmm0
    movsd val2(%rip), %xmm1
    mulsd %xmm1, %xmm0        /* xmm0 = pi * 2.0 */
    movsd val3(%rip), %xmm1
    addsd %xmm1, %xmm0        /* xmm0 = pi*2 + 1 ≈ 7.283 */
    movsd %xmm0, -8(%rbp)     /* 存到栈上局部变量, 演示 rbp 寻址 */

    lea msg_pi(%rip), %rcx
    movsd -8(%rbp), %xmm0     /* 值放 XMM0 (规范要求) */
    movq %xmm0, %rdx          /* ⚠️ Windows: UCRT printf 从 RDX 读浮点位模式! */
    mov $1, %eax               /* AL = 1: 用了 1 个 XMM 寄存器 */
    call printf

    /* === 练习 2: 浮点数组求和 === */
    lea farray(%rip), %rsi
    xorpd %xmm0, %xmm0
    mov $farray_len, %rcx
    xor %rdx, %rdx
.fsum_loop:
    addsd (%rsi,%rdx,8), %xmm0
    inc %rdx
    cmp %rcx, %rdx
    jl .fsum_loop
    movsd %xmm0, -16(%rbp)

    lea msg_fsum(%rip), %rcx
    movsd -16(%rbp), %xmm0
    movq %xmm0, %rdx          /* 位模式 -> RDX (Windows variadic 规则) */
    mov $1, %eax
    call printf

    /* === 练习 3: 浮点比较 === */
    movsd val1(%rip), %xmm0
    movsd val2(%rip), %xmm1
    comisd %xmm1, %xmm0
    ja .pi_greater
    lea no_str(%rip), %rdx
    jmp .print_cmp
.pi_greater:
    lea yes_str(%rip), %rdx
.print_cmp:
    lea msg_cmp(%rip), %rcx
    xor %eax, %eax
    call printf

    /* return 0 */
    xor %eax, %eax
    add $48, %rsp
    pop %rbp
    ret
