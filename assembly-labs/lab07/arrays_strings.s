/*
 * Lab 07: 数组与字符串
 * Windows MSYS2 UCRT64 — Microsoft x64 ABI
 *
 * 要点: RDI/RSI 在 Windows 上是 callee-saved!
 *       函数内把它们当指针推进没问题 (参数本来就是自己的),
 *       但 main 里用完必须恢复, 所以 main 压栈保存
 */

.section .rodata
msg_strlen: .asciz "strlen(\"Hello, Assembly World!\") = %lld\n"
msg_strcmp: .asciz "strcmp result (equal) = %lld\n"
msg_rep:    .asciz "rep movsb copy: \"%s\"\n"

.section .data
src: .asciz "Hello, Assembly World!"
src_len = . - src              /* 包含 '\0' */

.section .bss
dst: .space 100

.section .text
.globl main
.include "asm_io.inc"

/* my_strlen: RCX = str* -> RAX = len (叶子函数) */
my_strlen:
    xor %eax, %eax
.strlen_loop:
    cmpb $0, (%rcx,%rax)
    je .strlen_done
    inc %rax
    jmp .strlen_loop
.strlen_done:
    ret

/* my_strcmp: RCX = s1*, RDX = s2* -> RAX = diff
 * 直接推进参数寄存器 RCX/RDX, 不需要额外指针
 * ⚠️ 寄存器别名陷阱: %cl 是 %rcx 的低字节! RCX 正当指针用时,
 *    第二个字节必须换用 %r8b, 否则每轮循环指针都被悄悄破坏 */
my_strcmp:
.strcmp_loop:
    mov (%rcx), %al
    mov (%rdx), %r8b
    cmp %r8b, %al
    jne .strcmp_diff
    test %al, %al
    jz .strcmp_equal
    inc %rcx
    inc %rdx
    jmp .strcmp_loop
.strcmp_diff:
    movzbq %al, %rax
    movzbq %r8b, %r8
    sub %r8, %rax
    ret
.strcmp_equal:
    xor %eax, %eax
    ret

main:
    push %rbp
    mov %rsp, %rbp
    push %rbx
    push %rdi                  /* RDI/RSI 是 callee-saved, 用完要还 */
    push %rsi
    sub $40, %rsp              /* 3 个 push (奇数), 40 = 32 shadow + 8 补齐 */

    /* --- strlen --- */
    lea src(%rip), %rcx
    call my_strlen
    mov %rax, %rbx
    PRINT_INT msg_strlen, %rbx

    /* --- rep movsb 复制 --- */
    lea dst(%rip), %rdi
    lea src(%rip), %rsi
    mov $src_len, %rcx
    cld
    rep movsb

    lea dst(%rip), %rax        /* 宏要的是寄存器, 先 lea 到 RAX */
    PRINT_INT msg_rep, %rax

    /* --- strcmp --- */
    lea src(%rip), %rcx
    lea dst(%rip), %rdx
    call my_strcmp
    mov %rax, %rbx
    PRINT_INT msg_strcmp, %rbx

    /* return 0 */
    xor %eax, %eax
    add $40, %rsp
    pop %rsi
    pop %rdi
    pop %rbx
    pop %rbp
    ret
