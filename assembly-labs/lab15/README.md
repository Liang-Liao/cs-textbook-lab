# Lab 15: Shellcode 编写

## 目标
- 理解什么是 Shellcode
- 学会用 C 加载并执行机器码
- 理解 VirtualAlloc 分配可执行内存
- 体会 shellcode 与平台 ABI 的绑定关系

## 什么是 Shellcode？

Shellcode 是一段**直接由 CPU 执行的机器码字节**，通常用于：
- 安全研究和 CTF 竞赛
- 漏洞利用（缓冲区溢出）的原理学习
- 理解"代码就是数据"这一根本事实

## 关键代码片段

```c
// 分配 RWX 可执行内存 (Windows)
void *mem = VirtualAlloc(NULL, size,
    MEM_COMMIT | MEM_RESERVE,
    PAGE_EXECUTE_READWRITE);

// 复制机器码并执行
memcpy(mem, shellcode, size);
int (*func)() = (int (*)())mem;
int result = func();      // CPU 直接跳进这段字节!
```

```c
// ⚠️ 用字节数组而不是字符串字面量:
unsigned char sc[] = { 0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3 };
// sizeof(sc) == 6, 恰好等于字节数
// 若写成 "\xb8\x2a...\xc3" 字符串, sizeof 会多 1 (结尾 '\0'),
// memcpy 多复制 1 字节 — 长度隐患的经典来源
```

## 两个 shellcode（都遵守 Windows x64 ABI）

```c
/* 无参版: mov eax,42; ret — 6 字节 */
unsigned char sc_ret42[] = { 0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3 };

/* 带参版: mov eax,ecx; add eax,edx; ret — 5 字节
 * 参数在 RCX/RDX (Microsoft x64)。
 * Linux System V 参数在 RDI/RSI (89 F8 01 F7 C3),
 * 同一段字节在 Linux 上会算错 — shellcode 是平台绑定的! */
unsigned char sc_add[] = { 0x8B, 0xC1, 0x03, 0xC2, 0xC3 };
```

## 练习

### 练习 1: 运行观察
```bash
mingw32-make run
# sc_ret42 size: 6 bytes
# Executing shellcode...
# Shellcode returned: 42
#
# sc_add size: 5 bytes
# func(40, 2) = 42
# func(100, 200) = 300
```

### 练习 2: 用汇编器生成 shellcode
写一个 .s 文件，用 `objdump -d` 拿到字节序列，替换 sc_add 的内容——
体会"汇编 → 机器码"就是查编码表

### 练习 3: 立即数放参数
写 shellcode `mov eax, 1234; ret`，用 GDB `x/6xb mem` 核对字节

## ⚠️ 安全警告
这些知识仅用于学习和合法的安全研究。不要用于未授权的系统攻击。

## 验证

```bash
mingw32-make build && mingw32-make test
```

## 练习参考答案

> ⚠️ 剧透警告：先自己动手，再看答案！

### 练习 2
```asm
/* add.s — 参数 RCX/RDX, 返回 RAX */
    .text
    .globl _start
_start:
    mov %ecx, %eax
    add %edx, %eax
    ret
```
```bash
gcc -c add.s -o add.o
objdump -d add.o    # 8b c1  03 c2  c3 — 和 sc_add 完全一致
```

### 练习 3
```c
unsigned char sc_imm[] = { 0xB8, 0xD2, 0x04, 0x00, 0x00, 0xC3 };
/* mov eax, 1234 → B8 + 32 位小端立即数: 1234 = 0x04D2 → D2 04 00 00 */
```
`x/6xb mem` 输出 `0xb8 0xd2 0x04 0x00 0x00 0xc3`——
**x86 是小端序**，立即数低位在前。这就是你亲眼看到的"指令编码"。
