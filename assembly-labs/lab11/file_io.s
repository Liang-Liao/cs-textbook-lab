/*
 * Lab 11: 文件 I/O
 * Windows MSYS2 UCRT64 — Microsoft x64 ABI
 *
 * 用汇编直接调用 C 库: fopen / fprintf / fgets / fclose
 * FILE* 指针跨调用保存: 放 callee-saved 的 RBX
 */

.section .rodata
filename:   .asciz "output.txt"
mode_w:     .asciz "w"
mode_r:     .asciz "r"
content:    .asciz "Hello from Assembly!\nThis is line 2.\nWritten with fprintf.\n"
fmt_read:   .asciz "Read back: %s"
fmt_err:    .asciz "Error opening file!\n"
fmt_ok:     .asciz "File written successfully.\n"

.section .bss
read_buf:   .space 256

.section .text
.globl main

main:
    push %rbp
    mov %rsp, %rbp
    push %rbx                  /* FILE* 存这里 */
    sub $40, %rsp

    /* === fopen("output.txt", "w") === */
    lea filename(%rip), %rcx
    lea mode_w(%rip), %rdx
    call fopen
    test %rax, %rax
    jz .error
    mov %rax, %rbx             /* FILE* 保存到 rbx */

    /* === fprintf(fp, content) === */
    mov %rbx, %rcx
    lea content(%rip), %rdx
    xor %eax, %eax
    call fprintf

    /* === fclose(fp) === */
    mov %rbx, %rcx
    call fclose

    lea fmt_ok(%rip), %rcx
    xor %eax, %eax
    call printf

    /* === fopen("output.txt", "r") === */
    lea filename(%rip), %rcx
    lea mode_r(%rip), %rdx
    call fopen
    test %rax, %rax
    jz .error
    mov %rax, %rbx

    /* === fgets(buf, 256, fp): 第 3 个参数走 R8 === */
    lea read_buf(%rip), %rcx
    mov $256, %rdx
    mov %rbx, %r8
    call fgets

    /* === printf("Read back: %s", buf) === */
    lea fmt_read(%rip), %rcx
    lea read_buf(%rip), %rdx
    xor %eax, %eax
    call printf

    /* === fclose(fp) === */
    mov %rbx, %rcx
    call fclose

    /* return 0 */
    xor %eax, %eax
    jmp .done

.error:
    lea fmt_err(%rip), %rcx
    xor %eax, %eax
    call printf
    mov $1, %eax

.done:
    add $40, %rsp
    pop %rbx
    pop %rbp
    ret
