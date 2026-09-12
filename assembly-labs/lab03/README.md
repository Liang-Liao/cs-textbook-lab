# Lab 03: 算术运算与标志位

## 目标
- 掌握 ADD/SUB/MUL/DIV 指令
- 理解 CPU 标志位（FLAGS 寄存器）
- 学会有符号和无符号运算的区别

## 标志位速查

| 标志 | 名称 | 含义 |
|------|------|------|
| ZF | Zero Flag | 结果为 0 时置 1 |
| CF | Carry Flag | 无符号溢出 |
| SF | Sign Flag | 结果为负（最高位=1） |
| OF | Overflow Flag | 有符号溢出 |

## 指令速查 (AT&T 语法)

```asm
add $10, %rax       # rax += 10
sub %rbx, %rax      # rax -= rbx
inc %rax            # rax++
dec %rax            # rax--
neg %rax            # rax = -rax

# 无符号乘法
mul %rbx            # RDX:RAX = RAX * RBX (128 位结果)

# 有符号乘法
imul $3, %rax       # rax *= 3
imul %rbx, %rax     # rax = rax * rbx

# 无符号除法
xor %edx, %edx      # ⚠️ 必须先清零 RDX (被除数高半部分)!
div %rbx            # RAX = RDX:RAX / RBX, RDX = 余数
```

## 关键陷阱

```asm
# ❌ 除法前不清零 RDX → 垃圾高位 → 除法结果错乱甚至 #DE 异常
mov $100, %rax
mov $7, %rbx
div %rbx

# ✅ 正确做法
mov $100, %rax
xor %edx, %edx      # 先清零！
mov $7, %rbx
div %rbx            # RAX=14 (商), RDX=2 (余数)
```

## Windows ABI 陷阱：RDX 是第 2 参数寄存器

除法结果（商在 RAX、余数在 RDX）要传给 printf 时：
- 余数必须**先挪走**（RDX 马上要装第 2 参数）
- 商可以从 RAX 复制到 RDX

```asm
div %rbx            # RAX=14, RDX=2
mov %rdx, %r8       # 余数先挪到第 3 参数寄存器
mov %rax, %rbx      # 商存 callee-saved（后面的表达式还要用）
lea fmt(%rip), %rcx
mov %rbx, %rdx      # 商 -> 第 2 参数
xor %eax, %eax
call printf         # R8 里的余数随调用一起被消费
```

## 练习

### 练习 1: 运行观察
```bash
mingw32-make run
# 10 + 20 = 30
# 30 * 3 = 90
# 100 / 7 = 14 remainder 2
# (10+20)*3 - 100/7 = 76
```

### 练习 2: GDB 观察标志位
```bash
mingw32-make debug
# 在比较/算术指令后: info registers eflags
```

### 练习 3: 有符号除法
用 `idiv` 实现除法并处理负数被除数（注意符号扩展用 `cqto`）

## 验证

```bash
mingw32-make build && mingw32-make run
```

## 练习参考答案

> ⚠️ 剧透警告：先自己动手，再看答案！

### 练习 2
在 `div %rbx` 之后看 eflags：ZF=0（商非零）、CF/SF/OF 由商的位样决定。
在 `sub %rbx, %rax`（76）之后：结果为正 → SF=0；无借位 → CF=0。
GDB 里 `info registers eflags` 直接给文字解读（`[ PF AF SF ]` 之类）。

### 练习 3
```asm
    /* -100 / 7 (有符号) */
    mov $-100, %rax
    cqto                   /* 符号扩展 RAX -> RDX:RAX (有符号版清 RDX) */
    mov $7, %rbx
    idiv %rbx              /* RAX = -14, RDX = -2 (余数符号跟被除数) */
```
对比记忆：无符号 `div` 配 `xor %edx,%edx`；有符号 `idiv` 配 `cqto`。
