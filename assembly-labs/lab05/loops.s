/*
 * Lab 05: 循环
 * Windows MSYS2 UCRT64 — Microsoft x64 ABI
 *
 * 从本 Lab 开始使用 common/asm_io.inc 宏库:
 *   PRINT_INT fmt, reg  一行完成"装参数 + shadow space + 调 printf"
 *   宏展开是纯文本替换, 用 gcc -S 或 gdb 单步可以看到每一行
 */

.section .rodata
msg_sum:   .asciz "1+2+...+100 = %lld\n"
msg_fact:  .asciz "10! = %lld\n"
msg_fib:   .asciz "fib(20) = %lld\n"
msg_arrsum:.asciz "sum(1..10) = %lld\n"

.section .data
array:
    .quad 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
    array_len = (. - array) / 8

.section .text
.globl main
.include "asm_io.inc"

main:
    push %rbp
    mov %rsp, %rbp
    push %rbx
    push %r12
    sub $32, %rsp              /* shadow space: 2 个额外 push 后 RSP 已 16 对齐 */

    /* === 练习 1: 1+2+...+100 === */
    xor %rax, %rax
    mov $1, %rcx
.sum_loop:
    add %rcx, %rax             /* sum += i */
    inc %rcx
    cmp $100, %rcx
    jle .sum_loop
    mov %rax, %r12             /* 结果存 callee-saved, 跨宏调用安全 */
    PRINT_INT msg_sum, %r12

    /* === 练习 2: 10! === */
    mov $1, %rax
    mov $10, %rcx
.fact_loop:
    imul %rcx, %rax
    dec %rcx
    jnz .fact_loop
    mov %rax, %r12
    PRINT_INT msg_fact, %r12

    /* === 练习 3: fib(20) ===
     * 不变式: 进入循环体前 (rax,rbx) = (F(n), F(n+1)), rcx = n+1
     * rcx 从 2 数到 20, 循环体执行 19 次后 rbx = F(20) = 6765 */
    mov $0, %rax               /* F(0) */
    mov $1, %rbx               /* F(1) */
    mov $2, %rcx
.fib_loop:
    mov %rax, %rdx
    add %rbx, %rdx             /* rdx = F(n) + F(n+1) = F(n+2) */
    mov %rbx, %rax
    mov %rdx, %rbx
    inc %rcx
    cmp $20, %rcx
    jle .fib_loop
    mov %rbx, %r12
    PRINT_INT msg_fib, %r12

    /* === 练习 4: 数组求和 === */
    lea array(%rip), %rsi
    xor %rax, %rax
    xor %rdx, %rdx
.arr_loop:
    add (%rsi,%rdx,8), %rax    /* sum += array[i] (每个元素 8 字节) */
    inc %rdx
    cmp $array_len, %rdx
    jl .arr_loop
    mov %rax, %r12
    PRINT_INT msg_arrsum, %r12

    /* return 0 */
    xor %eax, %eax
    add $32, %rsp
    pop %r12
    pop %rbx
    pop %rbp
    ret
