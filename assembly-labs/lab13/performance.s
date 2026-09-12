/*
 * Lab 13: 性能优化 — 循环展开对比 (真实计时版)
 * Windows MSYS2 UCRT64 — Microsoft x64 ABI
 *
 * 计时用 Win32 API (kernel32, gcc 默认链接):
 *   QueryPerformanceCounter(RCX=指向 8 字节计数的指针)
 *   QueryPerformanceFrequency(RCX=指向每秒计数的指针)
 *
 * 数据放在 .comm (对应 .bss), 不占可执行文件体积;
 * 运行时先填充数组 (顺便暖缓存, 让计时更稳定)
 */

    .comm big_array, 8000000, 64   /* 100 万个 .quad, 未初始化 */

.section .rodata
msg_normal:   .asciz "Normal sum = %lld, time = %lld us\n"
msg_unrolled: .asciz "Unrolled sum = %lld, time = %lld us\n"

.section .data
freq:    .quad 0                    /* QPF: 每秒计数 */
t_start: .quad 0
t_end:   .quad 0
    array_len = 1000000

.section .text
.globl main
.include "asm_io.inc"

/* fill_array: 数组全部填 1 (叶子函数) */
fill_array:
    lea big_array(%rip), %rcx
    mov $array_len, %r8
    xor %edx, %edx
.fill_loop:
    movq $1, (%rcx,%rdx,8)
    inc %rdx
    cmp %r8, %rdx
    jl .fill_loop
    ret

/* sum_normal: 普通循环 (叶子函数) */
sum_normal:
    lea big_array(%rip), %rcx
    xor %eax, %eax
    xor %edx, %edx
.normal_loop:
    add (%rcx,%rdx,8), %rax
    inc %rdx
    cmp $array_len, %rdx
    jl .normal_loop
    ret

/* sum_unrolled4: 4 路展开, 每轮处理 4 个元素 */
sum_unrolled4:
    lea big_array(%rip), %rcx
    xor %eax, %eax
    xor %edx, %edx
.unrolled_loop:
    add (%rcx,%rdx,8), %rax
    add 8(%rcx,%rdx,8), %rax
    add 16(%rcx,%rdx,8), %rax
    add 24(%rcx,%rdx,8), %rax
    add $4, %rdx
    cmp $array_len, %rdx
    jl .unrolled_loop
    ret

/* elapsed_us: (t_end - t_start) * 1e6 / freq -> RAX = 微秒 */
elapsed_us:
    mov t_end(%rip), %rax
    sub t_start(%rip), %rax
    imul $1000000, %rax        /* 先乘后除, 保持精度 */
    xor %edx, %edx
    div freq(%rip)
    ret

main:
    push %rbp
    mov %rsp, %rbp
    push %rbx                  /* sum */
    push %r12                  /* time normal */
    push %r13                  /* sum unrolled */
    push %r14                  /* time unrolled */
    sub $32, %rsp              /* 4 个额外 push (偶数), RSP 已对齐 */

    /* QPF: 拿到计时器频率 */
    lea freq(%rip), %rcx
    call QueryPerformanceFrequency

    /* 填充数组 = 预热缓存 */
    call fill_array

    /* === 计时: 普通循环 === */
    lea t_start(%rip), %rcx
    call QueryPerformanceCounter
    call sum_normal
    mov %rax, %rbx
    lea t_end(%rip), %rcx
    call QueryPerformanceCounter
    call elapsed_us
    mov %rax, %r12
    PRINT_2INT msg_normal, %rbx, %r12

    /* === 计时: 4 路展开 === */
    lea t_start(%rip), %rcx
    call QueryPerformanceCounter
    call sum_unrolled4
    mov %rax, %r13
    lea t_end(%rip), %rcx
    call QueryPerformanceCounter
    call elapsed_us
    mov %rax, %r14
    PRINT_2INT msg_unrolled, %r13, %r14

    /* return 0 */
    xor %eax, %eax
    add $32, %rsp
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    ret
