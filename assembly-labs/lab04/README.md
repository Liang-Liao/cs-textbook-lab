# Lab 04: 条件与跳转

## 目标
- 掌握 CMP + 条件跳转实现 if-else
- 理解条件码和跳转指令的关系
- 学会写独立的函数（分段函数）

## 条件跳转指令 (AT&T)

| 指令 | 条件 | 含义 |
|------|------|------|
| je / jz | ZF=1 | 相等 / 为零 |
| jne / jnz | ZF=0 | 不等 / 非零 |
| jg | ZF=0 and SF=OF | 有符号大于 |
| jl | SF≠OF | 有符号小于 |
| jge | SF=OF | 有符号大于等于 |
| jle | ZF=1 or SF≠OF | 有符号小于等于 |
| ja | CF=0 and ZF=0 | 无符号大于 |
| jb | CF=1 | 无符号小于 |

## 关键代码片段

```asm
# 绝对值
mov $-42, %rax
test %rax, %rax      # test 设标志位但不修改 rax
jns .not_neg          # 非负则跳过
neg %rax              # 取反
.not_neg:

# 三个数最大值
cmp %rbx, %rax        # 比较 a 和 b
jge .a_ge_b
mov %rbx, %rax        # max = b
.a_ge_b:
```

## 自定义函数（Windows x64）

```asm
# 第 1 参数 = RCX, 返回值 = RAX
segment_func:
    mov %rcx, %rax
    test %rcx, %rcx
    js .seg_neg           # 符号位为 1 → 负数分支
    cmp $100, %rcx
    jge .seg_big
    add $10, %rax         # 0 <= x < 100
    ret
.seg_neg:
    imul $2, %rax
    ret
.seg_big:
    sub $50, %rax
    ret
```

叶子函数（内部不调用别人）不需要 shadow space，也不必动 RSP。
调用方：
```asm
    mov $50, %rcx         # 参数
    call segment_func
    mov %rax, %rdx        # 返回值 -> 第 2 参数（给 printf）
```

## 练习

### 练习 1: 运行观察
```bash
mingw32-make run
# abs(-42) = 42
# max(30, 50, 20) = 50
# f(50) = 60  (x+10 when 0<=x<100)
# f(-5) = -10  (x*2 when x<0)
# f(150) = 100  (x-50 when x>=100)
```

### 练习 2: 修改分段函数
改变分段函数的区间或公式，观察结果变化

### 练习 3: GDB 调试
```bash
mingw32-make debug
# 在 cmp 指令后: info registers eflags
# 观察 ZF/SF/CF/OF 的变化
```

## 验证

```bash
mingw32-make build && mingw32-make run
```

## 练习参考答案

> ⚠️ 剧透警告：先自己动手，再看答案！

### 练习 2（示例：四段函数）
```asm
/* g(x): x<0 -> 0; 0<=x<10 -> x; 10<=x<100 -> x*2; x>=100 -> 200 */
segment_func:
    mov %rcx, %rax
    test %rcx, %rcx
    js .g_zero
    cmp $10, %rcx
    jl .g_small
    cmp $100, %rcx
    jl .g_mid
    mov $200, %rax
    ret
.g_zero:
    xor %eax, %eax
    ret
.g_small:
    ret                   /* rax = x, 原样返回 */
.g_mid:
    add %rcx, %rax        /* rax = x*2 */
    ret
```
区间判断要**从小到大串行比较**，跳转目标就是"落到哪一段"。

### 练习 3
`cmp $100, %rcx` 在 x=50 时：50-100 = -50 → SF=1, OF=0 → jl 条件 SF≠OF 成立。
试试 x=150，观察 jl 不跳（SF=0, OF=0）。
