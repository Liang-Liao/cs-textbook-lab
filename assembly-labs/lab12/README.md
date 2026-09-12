# Lab 12: SIMD 向量化

## 目标
- 掌握 SSE 浮点并行指令
- 学会用 SIMD 做向量运算
- 学会向 printf 传 4 个浮点（含栈传参！）

## SIMD 寄存器

| 寄存器 | 宽度 | 用途 |
|--------|------|------|
| XMM0-15 | 128位 | SSE |
| YMM0-15 | 256位 | AVX |

```
XMM0 (128位) = 4个float 或 2个double
```

对齐要求：`movaps`（对齐版）要求内存地址 16 字节对齐，
数据定义时用 `.align 16`，存栈上时放 -16(%rbp)（RBP 本身 16 对齐）。

## 常用 SIMD 指令 (AT&T)

```asm
# 数据移动 (packed = 并行多个)
movaps  %xmm0, %xmm1       # 移动 128 位对齐数据
movaps  mem(%rip), %xmm0   # 从内存加载

# 并行算术
addps   %xmm1, %xmm0       # 4 个 float 并行加
mulpd   %xmm1, %xmm0       # 2 个 double 并行乘
addsd   %xmm1, %xmm0       # 单个 double 加 (标量)

# 水平求和 (把 4 个元素加到一起)
movaps  %xmm0, %xmm1
shufps  $0x4E, %xmm1, %xmm1  # 交换高低 64 位
addps   %xmm1, %xmm0
shufps  $0xB1, %xmm1, %xmm1  # 交换相邻 32 位
addss   %xmm1, %xmm0         # 低 32 位 = 总和
```

## 一次传 4 个浮点给 printf（本 lab 精华）

参数位置 2/3/4/5 → 前 3 个走寄存器、第 5 个走栈：

```asm
# f1..f3 的位模式 -> RDX/R8/R9 (Windows variadic 规则, 见 lab09)
movss -16(%rbp), %xmm0
cvtss2sd %xmm0, %xmm0
movq %xmm0, %rdx
movss -12(%rbp), %xmm0
cvtss2sd %xmm0, %xmm0
movq %xmm0, %r8
movss -8(%rbp), %xmm0
cvtss2sd %xmm0, %xmm0
movq %xmm0, %r9
# 第 5 参数走栈: shadow space (32 字节) 之上, 即 32(%rsp)
movss -4(%rbp), %xmm0
cvtss2sd %xmm0, %xmm0
movq %xmm0, 32(%rsp)

lea fmt(%rip), %rcx
mov $4, %eax               # AL = 4
call printf
```

栈布局（main 里 `sub $48, %rsp` 之后）：
```
[rsp+0..32)   shadow space
[rsp+32..40)  第 5 参数的栈槽   ← 就是这里!
```

## 练习

### 练习 1: 运行观察
```bash
mingw32-make run
# Vector add: [6.0, 8.0, 10.0, 12.0]
# Vector mul: [3.0, 8.0]
# Dot product = 70.0
```

### 练习 2: AVX 版本
把 addps 换成 AVX 的 256 位版本（数据改 .align 32，寄存器用 %ymm0，
编译加 -mavx）——4 个 float 变 8 个

### 练习 3: 手算 shufps
画出练习 3 每一步 shufps/addps 后 4 个 lane 的值，验证最终是 70

## 验证

```bash
mingw32-make build && mingw32-make test
```

## 练习参考答案

> ⚠️ 剧透警告：先自己动手，再看答案！

### 练习 2
```asm
    .align 32
vec8_a: .float 1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0
vec8_b: .float 8.0,7.0,6.0,5.0,4.0,3.0,2.0,1.0
```
```asm
vmovaps vec8_a(%rip), %ymm0
vaddps  vec8_b(%rip), %ymm0, %ymm0   # AVX 三操作数语法
```
编译: `gcc -g -mavx simd.s -o simd.exe`（或在 Makefile CFLAGS 加 -mavx）。
注意 AVX 指令是三操作数（目的可以不同于源），SSE 是二操作数。

### 练习 3
```
mulps 后:  xmm0 = [5, 12, 21, 32]
shufps $0x4E (01 00 11 10):  xmm1 = [21, 32, 5, 12]
addps:                        xmm0 = [26, 44, 26, 44]
shufps $0xB1 (10 11 00 01):  xmm1 = [44, 26, 44, 26]
addss:                        xmm0[0] = 26 + 44 = 70 ✓
```
两次 shuffle 让"对角线"元素配对相加——SIMD 水平归约的标准套路。
