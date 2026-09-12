/*
 * Lab 02: 寄存器与数据移动
 * Windows MSYS2 UCRT64 — Microsoft x64 ABI
 *
 * 栈帧模板 (本课程统一写法):
 *   进函数 RSP ≡ 8 (mod 16), push %rbp 后 ≡ 0
 *   之后再 push 奇数个寄存器 -> RSP ≡ 8: sub $40 (32 shadow + 8 补齐)
 *   之后再 push 偶数个寄存器 -> RSP ≡ 0: sub $32 (仅 32 shadow)
 */

.section .rodata
msg_val:  .asciz "val1 + val2 = %lld\n"
msg_fill: .asciz "RAX filled with -1:          0x%llx\n"
msg_size: .asciz "After 'mov eax, 0x12345678': rax = 0x%llx\n"

.section .data
val1:   .quad 100
val2:   .quad 200
result: .quad 0

.section .text
.globl main

main:
    push %rbp
    mov %rsp, %rbp
    push %rbx                  /* callee-saved: 跨 printf 保存中间值 */
    sub $40, %rsp              /* 32 shadow space + 8 补齐 16 对齐 */

    /* --- 练习 1: 立即数 -> 寄存器 --- */
    mov $42, %rax

    /* --- 练习 2: 寄存器间传递 --- */
    mov %rax, %rbx
    mov %rbx, %rcx

    /* --- 练习 3: 内存读写 --- */
    mov val1(%rip), %rax       /* 内存 -> 寄存器 */
    mov val2(%rip), %rbx
    add %rbx, %rax             /* RAX = 300 */
    mov %rax, result(%rip)     /* 寄存器 -> 内存 */

    lea msg_val(%rip), %rcx
    mov result(%rip), %rdx
    xor %eax, %eax
    call printf

    /* --- 练习 4: 数据大小: 32 位写入会清零高 32 位 --- */
    mov $-1, %rax              /* 第 1 步: 把 RAX 填满 1 -> 0xFFFFFFFFFFFFFFFF */
    mov %rax, %rbx

    lea msg_fill(%rip), %rcx
    mov %rbx, %rdx
    xor %eax, %eax
    call printf

    mov $0x12345678, %eax      /* 第 2 步: 32 位写入, 高 32 位自动清零 */
    lea msg_size(%rip), %rcx   /* 若不清零, 结果应是 0xFFFFFFFF12345678 */
    mov %rax, %rdx
    xor %eax, %eax
    call printf

    /* return 0 */
    xor %eax, %eax
    add $40, %rsp
    pop %rbx
    pop %rbp
    ret
