/*
 * Lab 04: 条件与跳转
 * Windows MSYS2 UCRT64 — Microsoft x64 ABI
 *
 * 要点: 自定义函数也遵守同一套约定 —
 *       第 1 参数放 RCX, 返回值放 RAX, 叶子函数可以不预留 shadow space
 */

.section .rodata
msg_abs:   .asciz "abs(-42) = %lld\n"
msg_max3:  .asciz "max(30, 50, 20) = %lld\n"
msg_func:  .asciz "f(50) = %lld  (x+10 when 0<=x<100)\n"
msg_neg:   .asciz "f(-5) = %lld  (x*2 when x<0)\n"
msg_big:   .asciz "f(150) = %lld  (x-50 when x>=100)\n"

.section .text
.globl main

main:
    push %rbp
    mov %rsp, %rbp
    sub $32, %rsp              /* shadow space */

    /* === 练习 1: 绝对值 === */
    mov $-42, %rax
    test %rax, %rax
    jns .abs_not_neg
    neg %rax
.abs_not_neg:
    lea msg_abs(%rip), %rcx
    mov %rax, %rdx
    xor %eax, %eax
    call printf

    /* === 练习 2: 三个数最大值 === */
    mov $30, %rax
    mov $50, %rdx              /* 借参数寄存器当临时变量 (下次调用前会重装) */
    mov $20, %r8

    cmp %rdx, %rax
    jge .a_ge_b
    mov %rdx, %rax
.a_ge_b:
    cmp %r8, %rax
    jge .a_ge_c
    mov %r8, %rax
.a_ge_c:
    lea msg_max3(%rip), %rcx
    mov %rax, %rdx
    xor %eax, %eax
    call printf

    /* === 练习 3: 分段函数 === */
    /* f(x) = x*2 if x<0, x+10 if 0<=x<100, x-50 if x>=100 */

    mov $50, %rcx              /* 第 1 参数 = x */
    call segment_func
    lea msg_func(%rip), %rcx
    mov %rax, %rdx             /* 返回值 -> 第 2 参数 */
    xor %eax, %eax
    call printf

    mov $-5, %rcx
    call segment_func
    lea msg_neg(%rip), %rcx
    mov %rax, %rdx
    xor %eax, %eax
    call printf

    mov $150, %rcx
    call segment_func
    lea msg_big(%rip), %rcx
    mov %rax, %rdx
    xor %eax, %eax
    call printf

    /* return 0 */
    xor %eax, %eax
    add $32, %rsp
    pop %rbp
    ret

/* 分段函数: RCX = x, 返回 RAX = f(x)
 * 叶子函数 (内部不调用别人): 不需要 shadow space, 也不必动 RSP */
segment_func:
    mov %rcx, %rax
    test %rcx, %rcx
    js .seg_neg
    cmp $100, %rcx
    jge .seg_big
    /* 0 <= x < 100 */
    add $10, %rax
    ret
.seg_neg:
    imul $2, %rax
    ret
.seg_big:
    sub $50, %rax
    ret
