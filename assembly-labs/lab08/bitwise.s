/*
 * Lab 08: 位运算
 * Windows MSYS2 UCRT64 — Microsoft x64 ABI
 */

.section .rodata
msg_even:    .asciz "42 is %s\n"
msg_count:   .asciz "popcount(0b10110100) = %lld\n"
msg_bits:    .asciz "After bts 5, btr 5, btc 3: rax = %lld\n"
msg_mul10:   .asciz "7 * 10 (via shifts) = %lld\n"
msg_swap:    .asciz "XOR swap: a=%lld, b=%lld\n"
odd_str:     .asciz "odd"
even_str:    .asciz "even"

.section .text
.globl main
.include "asm_io.inc"

main:
    push %rbp
    mov %rsp, %rbp
    push %rbx
    push %r12
    sub $32, %rsp              /* shadow space: 2 个额外 push 后 RSP 已 16 对齐 */

    /* === 练习 1: 奇偶判断 === */
    mov $42, %rax
    test $1, %rax
    jz .is_even
    lea odd_str(%rip), %rbx
    jmp .print_even
.is_even:
    lea even_str(%rip), %rbx
.print_even:
    PRINT_INT msg_even, %rbx

    /* === 练习 2: popcount (Brian Kernighan) === */
    mov $0b10110100, %rax       /* = 180, 有 4 个 1 */
    xor %r12, %r12             /* count = 0 */
.count_loop:
    test %rax, %rax
    jz .count_done
    mov %rax, %rbx
    dec %rbx
    and %rbx, %rax             /* 清除最低位的 1 */
    inc %r12
    jmp .count_loop
.count_done:
    PRINT_INT msg_count, %r12

    /* === 练习 3: 位域操作 === */
    mov $0, %rax
    bts $5, %rax               /* 设置第5位 → 32 */
    btr $5, %rax               /* 清除第5位 → 0 */
    btc $3, %rax               /* 切换第3位 → 8 */
    mov %rax, %r12
    PRINT_INT msg_bits, %r12

    /* === 练习 4: 快速乘10 === */
    /* x*10 = x*8 + x*2 = (x<<3) + (x<<1) */
    mov $7, %rax
    mov %rax, %rbx
    shl $3, %rax               /* x*8 = 56 */
    shl $1, %rbx               /* x*2 = 14 */
    add %rbx, %rax             /* 70 */
    mov %rax, %r12
    PRINT_INT msg_mul10, %r12

    /* === 练习 5: XOR 交换 === */
    mov $10, %rax
    mov $20, %rbx
    xor %rbx, %rax             /* a = a^b */
    xor %rax, %rbx             /* b = a^b^b = a */
    xor %rbx, %rax             /* a = a^b^a = b */
    /* rax=20, rbx=10 */
    mov %rax, %r12             /* 结果跨宏调用: r12 和 rbx 都是 callee-saved */
    PRINT_2INT msg_swap, %r12, %rbx

    /* return 0 */
    xor %eax, %eax
    add $32, %rsp
    pop %r12
    pop %rbx
    pop %rbp
    ret
