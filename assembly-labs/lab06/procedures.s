/*
 * Lab 06: 栈与过程调用
 * Windows MSYS2 UCRT64 — Microsoft x64 ABI
 *
 * 要点: 函数参数 RCX/RDX/R8/R9, 返回值 RAX
 *       printf 会破坏 volatile 寄存器: RAX, RCX, RDX, R8-R11, XMM0-XMM5
 *       跨调用的值放 callee-saved: RBX, RBP, RDI, RSI, R12-R15
 */

.section .rodata
msg_add:    .asciz "add_two(10, 20) = %lld\n"
msg_fact:   .asciz "factorial(10) = %lld\n"
msg_arrsum: .asciz "sum_array([10,20,30,40,50]) = %lld\n"

.section .data
array: .quad 10, 20, 30, 40, 50
array_len = 5

.section .text
.include "asm_io.inc"

/* ============================================
 * add_two: 返回 a + b
 * RCX = a, RDX = b -> RAX = a + b
 * ============================================ */
add_two:
    lea (%rcx,%rdx), %rax      /* 一条指令搞定 */
    ret

/* ============================================
 * factorial: 递归计算 n!
 * RCX = n -> RAX = n!
 * ============================================ */
factorial:
    push %rbp
    mov %rsp, %rbp
    push %rbx                  /* 保存 rbx (callee-saved) */
    sub $40, %rsp              /* 递归调用也要 shadow space + 16 对齐 */

    mov %rcx, %rbx             /* 保存 n */
    cmp $1, %rbx
    jle .fact_base

    /* 递归: n * factorial(n-1) */
    lea -1(%rbx), %rcx         /* n-1 -> 第 1 参数 */
    call factorial             /* rax = (n-1)! */
    imul %rbx, %rax            /* rax = n * (n-1)! */
    jmp .fact_done

.fact_base:
    mov $1, %rax

.fact_done:
    add $40, %rsp
    pop %rbx
    pop %rbp
    ret

/* ============================================
 * sum_array: 数组求和 (叶子函数)
 * RCX = arr*, RDX = len -> RAX = sum
 * ============================================ */
sum_array:
    xor %eax, %eax
    xor %r8d, %r8d             /* i = 0 (不用 RCX/RCX, 它已是参数) */
.sum_loop:
    cmp %rdx, %r8
    jge .sum_done
    add (%rcx,%r8,8), %rax
    inc %r8
    jmp .sum_loop
.sum_done:
    ret

/* ============================================
 * main
 * ============================================ */
.globl main
main:
    push %rbp
    mov %rsp, %rbp
    push %rbx
    sub $40, %rsp

    /* --- add_two(10, 20) --- */
    mov $10, %rcx
    mov $20, %rdx
    call add_two
    mov %rax, %rbx             /* 保存结果 (rbx 是 callee-saved) */
    PRINT_INT msg_add, %rbx

    /* --- factorial(10) --- */
    mov $10, %rcx
    call factorial
    mov %rax, %rbx
    PRINT_INT msg_fact, %rbx

    /* --- sum_array(array, 5) --- */
    lea array(%rip), %rcx
    mov $5, %rdx
    call sum_array
    mov %rax, %rbx
    PRINT_INT msg_arrsum, %rbx

    /* return 0 */
    xor %eax, %eax
    add $40, %rsp
    pop %rbx
    pop %rbp
    ret
