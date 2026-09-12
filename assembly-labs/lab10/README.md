# Lab 10: C 与汇编互操作 ⭐ 极其实用

## 目标
- 学会从 C 调用汇编函数
- 掌握 GCC 内联汇编
- 理解 Windows x64 ABI 的实战意义

## 为什么重要

实际项目中，你**几乎不会**写纯汇编程序。通常是：
1. 性能关键路径用汇编优化
2. 操作系统内核需要汇编（中断处理、上下文切换）
3. 逆向工程需要读懂编译器输出

## C 调用汇编（Windows x64）

```asm
# asm_funcs.s — 参数 RCX, 返回值 RAX
.globl asm_strlen
asm_strlen:
    push %rdi              # RDI 是 callee-saved, 且 scas 只能用 RDI
    mov %rcx, %rdi         # 参数 -> 扫描指针
    xor %eax, %eax
    or $-1, %rcx           # 扫描计数 = -1
    repnz scasb            # 找 '\0'
    not %rcx               # RCX = len + 2
    lea -1(%rcx), %rax     # RAX = len
    pop %rdi
    ret
```

```c
// main.c — 声明必须和汇编实现一致
extern size_t asm_strlen(const char *str);
extern long long asm_max(long long a, long long b);
```

```bash
gcc -g -c asm_funcs.s -o asm_funcs.o   # 编译汇编
gcc -g -c main.c -o main.o             # 编译 C
gcc -g -o main.exe main.o asm_funcs.o  # 链接 (或 mingw32-make build)
```

## ⚠️ LLP64：Windows 上 long 是 32 位！

| 类型 | Linux (LP64) | Windows (LLP64) |
|------|--------------|-----------------|
| long | 8 字节 | **4 字节** |
| long long | 8 字节 | 8 字节 |
| 格式符 | %ld | **%lld** |

汇编侧元素是 8 字节（.quad），C 侧必须用 `long long`（或 int64_t），
否则数组步长不匹配、点积算出来是乱的。这是 Linux→Windows 移植的
头号暗坑。

## 内联汇编

```c
long long a = 100, b = 200, sum;
asm volatile (
    "movq %1, %0\n\t"      // sum = a
    "addq %2, %0"          // sum += b
    : "=r" (sum)            // 输出: %0
    : "r" (a), "r" (b)      // 输入: %1, %2
);
```
`"r"` 约束让编译器自己挑寄存器——不写死 rdi/rsi，
同一份代码在 Windows/Linux 上都成立。

## 练习

### 练习 1: 运行观察
```bash
mingw32-make run
# C strlen:    16
# asm_strlen:  16
# asm_max(10, 20) = 20
# asm_dot_product: 70
# Inline asm: 100 + 200 = 300
```

### 练习 2: 在 C 里检查约定
在 GDB 中 `break asm_strlen; run`，停下时 `info registers rcx rdi`，
确认字符串指针在 RCX（System V 会在 RDI）

### 练习 3: 追加函数
写 `asm_sum3(a, b, c)` 返回三数之和——第 3 个参数用 R8

## 验证

```bash
mingw32-make build && mingw32-make test
```

## 练习参考答案

> ⚠️ 剧透警告：先自己动手，再看答案！

### 练习 2
```
(gdb) break asm_strlen
(gdb) run
(gdb) info registers rcx rdi
rcx   0x...   ← 字符串地址在这里
rdi   0x...   ← 还是调用前的值 (还没被我们改)
```

### 练习 3
```asm
/* asm_sum3: RCX = a, RDX = b, R8 = c -> RAX = a+b+c */
.globl asm_sum3
asm_sum3:
    mov %rcx, %rax
    add %rdx, %rax
    add %r8, %rax
    ret
```
```c
extern long long asm_sum3(long long a, long long b, long long c);
printf("%lld\n", asm_sum3(1, 2, 3));   // 6
```
第 4 个参数在 R9，第 5 个开始压栈——试试 asm_sum5 复习栈传参。
