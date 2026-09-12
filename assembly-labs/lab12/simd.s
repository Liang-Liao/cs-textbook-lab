/*
 * Lab 12: SIMD 向量化
 * Windows MSYS2 UCRT64 — Microsoft x64 ABI
 *
 * 要点 1: printf 一次传 4 个浮点 = 3 个寄存器 + 1 个栈槽 (第 5 参数,
 *         放在 shadow space 之上: 32(%rsp))
 * 要点 2: Windows variadic 从整型参数寄存器读浮点位模式 (lab09 已验证),
 *         所以每个浮点都要 movq 复制到 RDX/R8/R9
 */

.section .rodata
msg_vecadd: .asciz "Vector add: [%.1f, %.1f, %.1f, %.1f]\n"
msg_vecmul: .asciz "Vector mul: [%.1f, %.1f]\n"
msg_dot:    .asciz "Dot product = %.1f\n"

.section .data
    .align 16
vec_a: .float 1.0, 2.0, 3.0, 4.0
vec_b: .float 5.0, 6.0, 7.0, 8.0
    .align 16
dvec_a: .double 1.0, 2.0
dvec_b: .double 3.0, 4.0

.section .text
.globl main

main:
    push %rbp
    mov %rsp, %rbp
    sub $48, %rsp              /* 32 shadow + 8 (第 5 参数栈槽) + 8 补齐 */

    /* === 练习 1: 4 路 float 向量加法 === */
    movaps vec_a(%rip), %xmm0
    movaps vec_b(%rip), %xmm1
    addps %xmm1, %xmm0        /* xmm0 = [6,8,10,12] */
    movaps %xmm0, -16(%rbp)   /* 结果存栈上 (rbp 16 对齐, movaps 要求) */

    /* 逐个取出转 double 后传参: 第 2/3/4 参数走 RDX/R8/R9 */
    movss -16(%rbp), %xmm0
    cvtss2sd %xmm0, %xmm0
    movq %xmm0, %rdx
    movss -12(%rbp), %xmm0
    cvtss2sd %xmm0, %xmm0
    movq %xmm0, %r8
    movss -8(%rbp), %xmm0
    cvtss2sd %xmm0, %xmm0
    movq %xmm0, %r9
    /* 第 5 参数走栈: [rsp+32] (shadow space 之上) */
    movss -4(%rbp), %xmm0
    cvtss2sd %xmm0, %xmm0
    movq %xmm0, 32(%rsp)

    lea msg_vecadd(%rip), %rcx
    mov $4, %eax               /* AL = 4 */
    call printf

    /* === 练习 2: 2 路 double 向量乘法 === */
    movapd dvec_a(%rip), %xmm0
    movapd dvec_b(%rip), %xmm1
    mulpd %xmm1, %xmm0        /* xmm0 = [3.0, 8.0] */
    movapd %xmm0, -16(%rbp)

    movsd -16(%rbp), %xmm0
    movq %xmm0, %rdx           /* 第 2 参数 */
    movsd -8(%rbp), %xmm0
    movq %xmm0, %r8            /* 第 3 参数 */

    lea msg_vecmul(%rip), %rcx
    mov $2, %eax
    call printf

    /* === 练习 3: 点积 (shufps 水平求和) === */
    movaps vec_a(%rip), %xmm0
    movaps vec_b(%rip), %xmm1
    mulps %xmm1, %xmm0        /* [5,12,21,32] */
    movaps %xmm0, %xmm1
    shufps $0x4E, %xmm1, %xmm1 /* [21,32,5,12] */
    addps %xmm1, %xmm0        /* [26,44,26,44] */
    movaps %xmm0, %xmm1
    shufps $0xB1, %xmm1, %xmm1 /* [44,26,44,26] */
    addss %xmm1, %xmm0        /* xmm0[0] = 70.0 */
    cvtss2sd %xmm0, %xmm0     /* 转 double */

    movq %xmm0, %rdx
    lea msg_dot(%rip), %rcx
    mov $1, %eax
    call printf

    /* return 0 */
    xor %eax, %eax
    add $48, %rsp
    pop %rbp
    ret
