/*
 * Lab 16: 迷你 Bootloader
 *
 * ⚠️ 需要 QEMU 运行 (make run):
 *   pacman -S mingw-w64-ucrt-x86_64-qemu
 *
 * 工具链说明: 不用 ld — UCRT64 的 ld 只认 PE 格式。
 *   as --32 出 32 位 COFF, objcopy -O binary 直接抽取 .text 成裸二进制。
 *   因此代码以 0 为原点汇编: 数据引用必须用 DS=CS 段基址寻址。
 *
 * 编译: make build → boot.bin (512 字节引导扇区)
 */

.code16

.section .text
.globl _start

_start:
    ljmp $0x07C0, $real_start  /* BIOS 可能从 0000:7C00 跳入, 先把 CS 规范成 07C0 */

real_start:
    xor %ax, %ax
    mov %ax, %es               /* ES = 0 */
    mov %ax, %ss
    mov %cs, %ax
    mov %ax, %ds               /* DS = CS = 07C0: 标签按段内偏移(0 起点)寻址 */
    mov $0x7C00, %sp           /* 栈放在引导扇区下方 */

    /* 清屏 */
    mov $0x00, %ah
    mov $0x03, %al
    int $0x10

    /* 设置光标位置 */
    mov $0x02, %ah
    mov $0x00, %bh
    mov $10, %dh
    mov $35, %dl
    int $0x10

    /* 打印字符串 */
    lea msg, %si
    call print_string

    mov $0x02, %ah
    mov $0x00, %bh
    mov $12, %dh
    mov $30, %dl
    int $0x10

    lea msg2, %si
    call print_string

.halt:
    cli
    hlt
    jmp .halt

print_string:
    pusha
.ps_loop:
    lodsb                      /* AL = [DS:SI], SI++ */
    test %al, %al
    jz .ps_done
    mov $0x0E, %ah
    mov $0x00, %bh
    int $0x10
    jmp .ps_loop
.ps_done:
    popa
    ret

msg:  .asciz "Hello Boot!"
msg2: .asciz "Welcome to Assembly!"

.fill 510 - (. - _start), 1, 0
.word 0xAA55                  /* 引导签名: 最后两字节必须是 55 AA */
