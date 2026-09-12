# Lab 06: 栈与过程调用 ⭐ 核心章节

## 目标
- 理解 x86-64 的栈机制
- 掌握 CALL/RET 和函数编写
- 学会递归

## 栈帧结构（Windows x64）

```
高地址 ┌──────────────┐
       │  shadow space │  调用方为参数留的 32 字节
       │  返回地址      │  ← call 自动压入, RSP ≡ 8 (mod 16)
       │  旧 RBP       │  ← push %rbp 后 RBP 指向这里 (16 对齐)
       │  保存的 RBX   │
       │  局部变量      │  低地址
       └──────────────┘  ← RSP 指向这里
```

## 调用约定（Microsoft x64）

- **参数**: RCX, RDX, R8, R9（第 5 个起走栈）
- **返回值**: RAX
- **caller-saved（volatile，调用后被破坏）**: RAX, RCX, RDX, R8-R11, XMM0-XMM5
- **callee-saved（非 volatile，必须保护）**: RBX, RBP, RDI, RSI, R12-R15, XMM6-XMM15
- ⚠️ 与 System V 的关键差异：**RDI/RSI 在 Windows 上是 callee-saved**！

## 函数模板

```asm
my_func:                   # 入口 RSP ≡ 8 (mod 16)
    push %rbp              # RSP ≡ 0
    mov %rsp, %rbp
    push %rbx              # 保存 callee-saved (RSP ≡ 8)
    sub $40, %rsp          # 32 shadow + 8 补齐对齐 (RSP ≡ 0)
    # ... 函数体 ...
    add $40, %rsp
    pop %rbx
    pop %rbp
    ret
```

> 对齐速记：rbp 之外再 push **奇数**个寄存器 → `sub $40`；**偶数**个 → `sub $32`。

## 递归阶乘（本 lab 核心代码）

```asm
factorial:                 # RCX = n -> RAX = n!
    push %rbp
    mov %rsp, %rbp
    push %rbx              # 保存 callee-saved
    sub $40, %rsp          # 递归调用也需要 shadow space!

    mov %rcx, %rbx         # 保存 n (跨递归调用存活)
    cmp $1, %rbx
    jle .fact_base

    lea -1(%rbx), %rcx     # n-1 -> 第 1 参数
    call factorial         # rax = (n-1)!
    imul %rbx, %rax        # rax = n * (n-1)!
    jmp .fact_done

.fact_base:
    mov $1, %rax

.fact_done:
    add $40, %rsp
    pop %rbx
    pop %rbp
    ret
```

## ⚠️ 跨调用保存值的铁律

```asm
# ❌ 错误: 结果放 volatile 寄存器
call some_func
mov %rax, %rcx       # rcx 是 caller-saved
call printf          # rcx 被破坏!
mov %rcx, %rdx       # 💥 打印出垃圾

# ✅ 正确: 放 callee-saved
call some_func
mov %rax, %rbx       # rbx 是 callee-saved
call printf          # rbx 不受影响
mov %rbx, %rdx
```

## 练习

### 练习 1: 运行观察
```bash
mingw32-make run
# add_two(10, 20) = 30
# factorial(10) = 3628800
# sum_array([10,20,30,40,50]) = 150
```

### 练习 2: 写 swap 函数
写一个 `swap(long long *a, long long *b)` 汇编函数，让 C 侧调用验证

### 练习 3: 递归 fib
模仿 factorial 写递归版 `fib(n)`，和 lab05 的迭代版对比

### 练习 4: GDB 看栈
`make debug`，在 factorial 里用 `bt` 看递归调用链，`x/8gx $rsp` 看栈内容

## 验证

```bash
mingw32-make build && mingw32-make test
```

## 练习参考答案

> ⚠️ 剧透警告：先自己动手，再看答案！

### 练习 2
```asm
/* swap: RCX = a*, RDX = b* */
.globl swap
swap:
    mov (%rcx), %rax
    mov (%rdx), %r8       /* rax=*a, r8=*b (都是 volatile, 随便用) */
    mov %r8, (%rcx)
    mov %rax, (%rdx)
    ret
```
```c
extern void swap(long long *a, long long *b);
long long x = 1, y = 2;
swap(&x, &y);             // x=2, y=1
```

### 练习 3
```asm
/* fib: RCX = n -> RAX = F(n) */
fib:
    cmp $2, %rcx
    jge .fib_rec
    mov $1, %rax          /* F(1) = F(2) = 1 (n<=2 的基例) */
    ret
.fib_rec:
    push %rbp
    mov %rsp, %rbp
    push %rbx
    sub $40, %rsp
    mov %rcx, %rbx        /* 保存 n */
    dec %rcx
    call fib              /* F(n-1) */
    mov %rax, %r9         /* 暂存 (volatile, 马上要用) */
    lea -2(%rbx), %rcx
    call fib              /* F(n-2) */
    add %r9, %rax         /* F(n) = F(n-1) + F(n-2) */
    add $40, %rsp
    pop %rbx
    pop %rbp
    ret
```
递归 fib(20) 要做数万次调用，对比迭代的 19 次循环——体会指数复杂度。

### 练习 4
`bt` 在递归到最深处会显示 10 层 `#0 factorial ... #9 factorial`。
`x/8gx $rbp` 能看到 `[rbp]=旧rbp, [rbp+8]=返回地址`——和 README 里的栈帧图对上。
