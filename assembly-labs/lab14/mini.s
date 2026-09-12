/*
 * Lab 14: PE 可执行文件剖析
 * Windows MSYS2 UCRT64 — Microsoft x64 ABI
 *
 * Windows 上的产物是 PE/COFF 格式 (不是 ELF)。
 * 程序本身很小, 重头戏在 make analyze: 用 binutils 拆开看 PE 结构。
 */

.section .rodata
msg: .asciz "Minimal program!"
msg_info: .asciz "Run 'make analyze' to dissect the PE binary."

.section .text
.globl main
.include "asm_io.inc"

main:
    push %rbp
    mov %rsp, %rbp
    sub $32, %rsp

    PRINT_STR msg
    PRINT_STR msg_info

    xor %eax, %eax
    add $32, %rsp
    pop %rbp
    ret
