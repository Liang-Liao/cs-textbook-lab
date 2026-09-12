# Lab 02: 寄存器与数据移动

## 目标
- 理解 x86-64 的寄存器体系
- 掌握 MOV 指令的各种形式
- 理解数据大小（byte/word/dword/qword）

## 寄存器全景

### 通用寄存器（64 位）
```
RAX    累加器，函数返回值（caller-saved）
RCX    第 1 参数寄存器（caller-saved）
RDX    第 2 参数寄存器（caller-saved）
R8-R11 参数/临时（caller-saved）
RBX    callee-saved（跨函数调用保持不变）
RBP    栈帧基址（callee-saved）
RSP    栈指针（callee-saved）
RSI    callee-saved ⚠️ Windows 特有（Linux 上是参数/临时）
RDI    callee-saved ⚠️ Windows 特有（Linux 上是参数/临时）
R12-R15 callee-saved
```

### 寄存器大小访问
```
RAX (64位)
├── EAX (低32位)    ← 写 EAX 会清零高32位！
│   ├── AX (低16位)
│   │   ├── AL (低8位)
│   │   └── AH (高8位)
│   └─ (高16位不可直接访问)
└─ (高32位)
```

## 关键代码片段

```asm
# 立即数 → 寄存器
mov $42, %rax

# 内存读写
mov val1(%rip), %rax       # 读内存: rax = *val1
mov %rax, result(%rip)     # 写内存: *result = rax

# 数据大小注意（本 lab 的核心演示）
mov $-1, %rax              # RAX = 0xFFFFFFFFFFFFFFFF
mov $0x12345678, %eax      # 32 位写入，高 32 位自动清零
                           # RAX = 0x0000000012345678
```

先用 `-1` 把 RAX 填满，再 32 位写入——这样才能**看见**"高 32 位清零"的效果。

## 练习

### 练习 1: 运行并观察
```bash
mingw32-make run
# val1 + val2 = 300
# RAX filled with -1:          0xffffffffffffffff
# After 'mov eax, 0x12345678': rax = 0x12345678
```

### 练习 2: 修改数据
把 `val1` 和 `val2` 改成其他值，重新编译运行

### 练习 3: 16 位写入实验
模仿练习 4，验证"写 AX 只影响低 16 位"：填满 RAX 后执行 `mov $0x1111, %ax`，打印观察

## 验证

```bash
mingw32-make build && mingw32-make run
```

## 练习参考答案

> ⚠️ 剧透警告：先自己动手，再看答案！

### 练习 2
```asm
val1: .quad 150
val2: .quad 250           # 输出 val1 + val2 = 400
```

### 练习 3
```asm
    mov $-1, %rax
    mov $0x1111, %ax      /* 16 位写入: 只改低 16 位! */
    lea fmt_ax(%rip), %rcx
    mov %rax, %rdx
    xor %eax, %eax
    call printf
```
```asm
fmt_ax: .asciz "After 'mov ax, 0x1111': rax = 0x%llx\n"
# 输出: rax = 0xffffffffffff1111  ← 高 48 位原封不动
```
对比：32 位写入清零高 32 位，16/8 位写入保留高位——这是 x86 历史兼容设计。
