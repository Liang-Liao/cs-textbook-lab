# Lab 16: 迷你 Bootloader ⭐ 挑战级

## 目标
- 理解计算机启动过程
- 编写引导扇区代码
- 了解实模式 (Real Mode)

## 引导扇区基础

- BIOS 加载磁盘第一个扇区（512 字节）到物理地址 `0x7C00`
- 最后两个字节必须是 `0x55, 0xAA`（引导签名）
- 代码必须是 **16 位实模式**（`.code16`）
- BIOS 可能从 `0000:7C00` 跳入（CS=0），所以先 `ljmp $0x07C0, $real_start`
  规范化 CS，再让 `DS = CS`——这样所有数据标签按段内偏移（0 起点）寻址

## 本 lab 的特殊工具链

```
UCRT64 的 ld 只认 PE 格式, 链接不了 16 位 ELF 引导程序。
方案: 不用 ld!
  as --32 boot.s -o boot.o          # 32 位 COFF 对象
  objcopy -O binary -j .text boot.o boot.bin   # 抽取 .text 成裸二进制
```

裸二进制 = 从 0 偏移开始的纯字节，正好是 BIOS 要加载的扇区内容。

## 实模式 vs 长模式

| 特性 | 实模式 (16位) | 长模式 (64位) |
|------|--------------|--------------|
| 地址空间 | 1MB | 256TB |
| 寻址方式 | 段:偏移 (段×16+偏移) | 平坦地址 |
| 寄存器 | AX,BX,CX,DX | RAX,RBX,... |
| 内存保护 | 无 | 有 |

## BIOS 中断

| 中断 | 功能 |
|------|------|
| INT 0x10, AH=0x0E | 显示字符 (teletype) |
| INT 0x10, AH=0x03 | 清屏/设光标 |
| INT 0x13 | 磁盘读写 |
| INT 0x16 | 键盘输入 |

## 编译运行

```bash
mingw32-make build      # 编译 → boot.bin (512 字节)
mingw32-make test       # 校验大小和 55AA 签名
mingw32-make run        # QEMU 启动 (Ctrl+A, X 退出)
```

## 额外依赖

```bash
pacman -S mingw-w64-ucrt-x86_64-qemu
```

## 验证

```bash
mingw32-make build && mingw32-make test
# PASS boot (512 bytes, 55AA signature)
# 运行: 屏幕两行 "Hello Boot!" / "Welcome to Assembly!"
```

## 练习

### 练习 1: 改输出
把打印的文案改成自己的，行号/列号改改看（dh=行, dl=列）

### 练习 2: 16 进制打印
用 int 0x10 打印一个字节的 16 进制（自己实现 nibble → ASCII 转换）

### 练习 3: 从磁盘读第二个扇区
用 int 0x13 (AH=0x02) 把第 2 扇区读到 0x7E00 并跳过去执行——
这是"多扇区 bootloader"的第一步

## 练习参考答案

> ⚠️ 剧透警告：先自己动手，再看答案！

### 练习 1
改 `mov $12, %dh` / `mov $30, %dl`（0 起。光标位置不对会叠在上一行）。

### 练习 2
```asm
/* AL = 要打印的字节, 打印 2 个 16 进制字符 */
print_hex_byte:
    pusha
    mov %al, %bl
    shr $4, %al          # 高 nibble
    call .put_nibble
    mov %bl, %al
    and $0x0F, %al       # 低 nibble
    call .put_nibble
    popa
    ret
.put_nibble:
    cmp $10, %al
    jb .digit
    add $('A'-10), %al   # A-F
    jmp .emit
.digit:
    add $'0', %al        # 0-9
.emit:
    mov $0x0E, %ah
    mov $0x00, %bh
    int $0x10
    ret
```

### 练习 3
```asm
    /* 读第 2 扇区 (CHS: 柱面0 磁头0 扇区2) 到 0000:7E00 */
    mov $0x02, %ah       # 功能: 读扇区
    mov $1, %al          # 读 1 个扇区
    xor %ch, %ch         # 柱面 0
    mov $2, %cl          # 扇区 2
    xor %dh, %dh         # 磁头 0
    xor %dl, %dl         # 驱动器 A:
    mov $0x07E0, %bx     # ES:BX = 07E0:0000 = 物理地址 0x7E00
    mov %bx, %es
    xor %bx, %bx
    int $0x13
    jc .disk_error       # CF=1 = 出错
    ljmp $0x07E0, $0     # 跳过去执行!
```
第二个扇区里要放第二段 `.code16` 代码，同样以 0 为原点汇编、
objcopy 抽出后拼在 512 字节之后。
