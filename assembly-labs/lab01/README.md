# Lab 01: Hello World

## 目标
- 理解汇编程序的基本结构
- 学会用 `printf` / `puts` 做输出
- 掌握 gcc 编译汇编的完整流程

## 知识点

### 程序结构（Windows x64 / Microsoft ABI）
```asm
.section .rodata          # 只读数据（字符串常量）
msg: .asciz "Hello!\n"    # 以 '\0' 结尾的字符串

.section .text            # 代码段
.globl main               # 入口点是 main（不是 _start）

main:
    push %rbp             # 进 main 时 RSP ≡ 8 (mod 16)，push 后 ≡ 0
    mov %rsp, %rbp
    sub $32, %rsp         # shadow space：调用前必须给参数留 32 字节

    lea msg(%rip), %rcx   # 第 1 个参数 = RCX（Linux System V 用 RDI）
    call puts             # 调用 C 库的 puts

    xor %eax, %eax        # return 0
    add $32, %rsp
    pop %rbp
    ret
```

### Windows x64 调用约定三要素
1. **参数寄存器**：整型参数依次放 `RCX, RDX, R8, R9`，第 5 个起走栈
2. **shadow space**：调用方必须预留 32 字节（`sub $32, %rsp`），供被调方暂存寄存器参数
3. **栈对齐**：call 指令执行前 RSP 必须 16 字节对齐

### 为什么用 main + printf 而不是 _start + syscall？

| 方式 | 优点 | 缺点 |
|------|------|------|
| `_start` + `syscall` | 最底层，理解系统调用 | 系统调用号每平台不同，Windows 没有 syscall 指令接口 |
| `main` + `printf` | 走 C 运行时，Windows (UCRT) 直接可用 | 依赖 C 运行时 |

本课程选择后者，因为实际项目中汇编几乎都是和 C 混合使用。

### 编译流程
```bash
gcc -g hello.s -o hello.exe    # gcc 直接编译汇编 + 链接 C 运行时
./hello.exe                     # 运行
# 或者
mingw32-make build && mingw32-make run
```

## 练习

### 练习 1: 基础输出
运行 `mingw32-make run`，观察两行输出的来源（puts vs printf）

### 练习 2: 改进版
修改代码，输出你的名字：`Hello, I am [name]!`

### 练习 3: 多行输出
用多个 `printf` 调用输出多行文本

### 练习 4: 格式化输出
用 `printf` 的 `%lld` 格式符输出一个整数计算结果

## 验证

```bash
mingw32-make build && mingw32-make run
# Hello, Assembly!
# Hello, I am Student!
```

## 练习参考答案

> ⚠️ 剧透警告：先自己动手，再看答案！

### 练习 2
```asm
name:
    .asciz "Student"
```
只改数据，代码不动。

### 练习 3
```asm
    lea line1(%rip), %rcx
    call puts
    lea line2(%rip), %rcx
    call puts
```
```asm
line1: .asciz "First line"
line2: .asciz "Second line"
```

### 练习 4
```asm
    mov $6, %rax
    imul $7, %rax            /* rax = 42 */
    lea fmt_int(%rip), %rcx
    mov %rax, %rdx           /* 第 2 参数 */
    xor %eax, %eax           /* 无浮点参数 */
    call printf
```
```asm
fmt_int: .asciz "6 * 7 = %lld\n"
```
注意 Windows 上打印 64 位整数用 `%lld`（long long），不是 `%ld`——Windows 的 long 是 32 位！
