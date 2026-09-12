/*
 * Lab 03: 算术运算
 * Windows MSYS2 UCRT64 — Microsoft x64 ABI
 *
 * 要点 1: 无符号 DIV 用 RDX:RAX 作被除数, 除法前必须清零 RDX
 * 要点 2: Windows 下 RDX 同时是第 2 参数寄存器,
 *         除法结果要跨函数调用保存时, 先挪到 callee-saved 寄存器
 */

.section .rodata
msg_add:   .asciz "10 + 20 = %lld\n"
msg_mul:   .asciz "30 * 3 = %lld\n"
msg_div:   .asciz "100 / 7 = %lld remainder %lld\n"
msg_expr:  .asciz "(10+20)*3 - 100/7 = %lld\n"

.section .text
.globl main

main:
    push %rbp
    mov %rsp, %rbp
    push %rbx                  /* callee-saved */
    push %r12
    sub $32, %rsp              /* shadow space: 2 个额外 push 后 RSP 已 16 对齐 */

    /* === 10 + 20 === */
    mov $10, %rax
    add $20, %rax              /* RAX = 30 */
    mov %rax, %r12

    lea msg_add(%rip), %rcx
    mov %r12, %rdx
    xor %eax, %eax
    call printf

    /* === 30 * 3 === */
    mov %r12, %rax
    imul $3, %rax              /* RAX = 90 */
    mov %rax, %r12

    lea msg_mul(%rip), %rcx
    mov %r12, %rdx
    xor %eax, %eax
    call printf

    /* === 100 / 7 === */
    mov $100, %rax
    xor %edx, %edx             /* ⚠️ 除法前必须清零 RDX (被除数高半部分) */
    mov $7, %rbx
    div %rbx                   /* RAX = 14 (商), RDX = 2 (余数) */
    mov %rdx, %r8              /* 余数先挪到第 3 参数寄存器 R8 */
    mov %rax, %rbx             /* 商存入 callee-saved 的 RBX:
                                  RDX 马上要装第 2 参数, RAX 是 volatile,
                                  而最后的表达式 (90 - 商) 还要用它 */

    lea msg_div(%rip), %rcx
    mov %rbx, %rdx             /* 商 -> 第 2 参数 */
    xor %eax, %eax
    call printf                /* R8 里的余数随调用一起被消费 */

    /* === 完整表达式: (10+20)*3 - 100/7 === */
    mov %r12, %rax             /* 90 */
    sub %rbx, %rax             /* 90 - 14 = 76 */

    lea msg_expr(%rip), %rcx
    mov %rax, %rdx
    xor %eax, %eax
    call printf

    /* return 0 */
    xor %eax, %eax
    add $32, %rsp
    pop %r12
    pop %rbx
    pop %rbp
    ret
