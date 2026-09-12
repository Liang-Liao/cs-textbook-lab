# Lab 05: 循环

## 目标
- 用跳转指令实现各种循环模式
- 掌握计数器循环、条件循环
- 学会遍历数组
- **引入 GAS 宏：消灭 printf 样板代码**

## 循环模式 (AT&T)

### 计数器循环 (for)
```asm
    mov $10, %rcx          # 循环次数
.loop:
    # 循环体
    dec %rcx
    jnz .loop              # rcx != 0 则继续
```

### 条件循环 (while)
```asm
.loop:
    # 计算条件
    cmp $100, %rax
    jge .end               # 条件不满足则退出
    # 循环体
    jmp .loop
.end:
```

## GAS 宏（本 lab 起启用）

前面 4 个 lab 里，每次调 printf 都要写 5 行样板（装 RCX/RDX、清 AL、
shadow space……）。从本 lab 开始用 `common/asm_io.inc` 的宏：

```asm
.section .text
.globl main
.include "asm_io.inc"      # 引入宏库 (Makefile 已配好搜索路径)

    PRINT_INT msg_sum, %r12   # 一行 = 装参数 + shadow space + call printf
```

| 宏 | 等价于 |
|----|--------|
| `PRINT_STR label` | `puts(字符串)` |
| `PRINT_INT fmt, reg` | `printf(fmt, reg)` — 整数或 %s |
| `PRINT_2INT fmt, r1, r2` | `printf(fmt, r1, r2)` |
| `PRINT_DBL fmt` | `printf(fmt, xmm0)` — 值先放 XMM0 |

宏只是**汇编期文本替换**：`gcc -S` 或在 GDB 里单步 `si`，能看到它展开成的每条指令。
自己写 5 行裸版和写宏，生成的机器码完全一样。

## 关键代码片段

```asm
# 斐波那契: 不变式 (rax,rbx) = (F(n), F(n+1))
mov $0, %rax              # F(0)
mov $1, %rbx              # F(1)
.fib_loop:
    mov %rax, %rdx
    add %rbx, %rdx        # rdx = F(n) + F(n+1)
    mov %rbx, %rax
    mov %rdx, %rbx
    inc %rcx
    cmp $20, %rcx         # rcx 数到 20 → rbx = F(20)
    jle .fib_loop

# 数组遍历
lea array(%rip), %rsi
add (%rsi,%rdx,8), %rax   # sum += array[i] (每个元素 8 字节)
```

## 练习

### 练习 1: 运行观察
```bash
mingw32-make run
# 1+2+...+100 = 5050
# 10! = 3628800
# fib(20) = 6765
# sum(1..10) = 55
```

### 练习 2: 修改循环次数
把 100 改成 1000，观察结果

### 练习 3: 宏展开观察
在 GDB 中对 `PRINT_INT` 宏展开后的指令逐条 `si` 单步

### 练习 4: 自己写宏
给宏库加一个 `PRINT_HEX fmt, reg`，用 `%llx` 打印 16 进制

## 验证

```bash
mingw32-make build && mingw32-make test
```

## 练习参考答案

> ⚠️ 剧透警告：先自己动手，再看答案！

### 练习 2
`cmp $1000, %rcx` → 1+2+...+1000 = 500500。
（10! 改 15! 就溢出了：15! = 1307674368000 未溢出，但 21! 超过 int64）

### 练习 3
`mingw32-make debug`，`break main` 后 `si` 若干次，会依次看到：
`lea fmt(%rip),%rcx` → `mov %r12,%rdx` → `xor %eax,%eax` → `sub $32,%rsp` →
`call printf` → `add $32,%rsp` —— 正是宏注释里承诺的"约定四件事"。

### 练习 4
在 `common/asm_io.inc` 末尾添加：
```asm
/* PRINT_HEX fmt, reg — 用 %llx 打印 16 进制 (等价 PRINT_INT, 语义别名) */
.macro PRINT_HEX fmt, val
    lea \fmt(%rip), %rcx
    mov \val, %rdx
    xor %eax, %eax
    sub $32, %rsp
    call printf
    add $32, %rsp
.endm
```
用法：`PRINT_HEX fmt_hex, %rbx`，配 `fmt_hex: .asciz "value = 0x%llx\n"`。
