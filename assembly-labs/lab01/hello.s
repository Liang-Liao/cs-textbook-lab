/*
 * Lab 01: Hello World
 * Windows MSYS2 UCRT64 — Microsoft x64 ABI, gcc 编译链接
 *
 * 编译运行:
 *   make build && make run
 *
 * 要点:
 *   - main 由 C 运行时调用, 用 ret 返回 (不是 syscall exit)
 *   - Windows x64 参数寄存器: RCX, RDX, R8, R9
 *   - 调用前必须: RSP 16 字节对齐 + 预留 32 字节 shadow space
 */

.section .rodata
msg:
    .asciz "Hello, Assembly!"
fmt:
    .asciz "Hello, I am %s!\n"
name:
    .asciz "Student"

.section .text
.globl main

main:
    push %rbp
    mov %rsp, %rbp             /* 进 main 时 RSP ≡ 8 (mod 16), push 后 ≡ 0 */
    sub $32, %rsp              /* shadow space (保持 RSP ≡ 0, 满足调用前 16 对齐) */

    /* === 方式1: puts 输出简单字符串 === */
    lea msg(%rip), %rcx        /* Windows: 第 1 个参数 = RCX (Linux System V 是 RDI) */
    call puts                  /* puts 自动加换行 */

    /* === 方式2: printf 格式化输出 === */
    lea fmt(%rip), %rcx        /* 格式字符串 -> 第 1 参数 */
    lea name(%rip), %rdx       /* %s 的值 -> 第 2 参数 */
    xor %eax, %eax             /* 无浮点参数: variadic 函数要求 AL = 用到的 XMM 个数 */
    call printf

    /* === 返回 0 === */
    xor %eax, %eax             /* return 0 */
    add $32, %rsp
    pop %rbp
    ret
