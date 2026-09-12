# Lab 09: 浮点运算

## 目标
- 掌握 SSE 浮点指令
- 学会浮点数的加载、运算、比较
- 理解 printf 浮点参数传递（Windows 特殊规则！）

## SSE 浮点指令 (AT&T)

```asm
# 数据加载
movsd val(%rip), %xmm0       # 加载双精度 (double)
movss val(%rip), %xmm0       # 加载单精度 (float)

# 算术运算
addsd %xmm1, %xmm0           # xmm0 += xmm1 (双精度)
mulsd %xmm1, %xmm0           # xmm0 *= xmm1

# 比较
comisd %xmm1, %xmm0          # 比较并设置 EFLAGS
ja .greater                   # xmm0 > xmm1

# 类型转换
cvtss2sd %xmm0, %xmm0        # float → double
cvttsd2si %xmm0, %rcx        # double → 整数 (截断)
```

## 传浮点参数给 printf（Windows 关键差异 ⚠️）

```asm
movsd val(%rip), %xmm0       # 值放 XMM0 (规范要求)
movq %xmm0, %rdx             # ⚠️ 位模式必须复制到 RDX!
mov $1, %eax                  # AL = 用到的 XMM 个数
call printf
```

**为什么多一步 movq？** UCRT 的 variadic 函数（printf/fprintf）在读取
可变参数时，是从整型参数寄存器的 home 槽位里取浮点位模式的——
这点和 System V（纯走 XMM 寄存器）完全相反。实测：
- 只设 XMM0 → 打印 `0.000000`
- XMM0 + 位模式复制到 RDX → 正确输出

`movq %xmm0, %rdx` 是 64 位整数传送（搬运位模式，不做类型转换），
与 `movsd`（浮点传送）是两条不同指令。

## 关键代码片段

```asm
# 浮点数组求和
lea farray(%rip), %rsi
xorpd %xmm0, %xmm0           # sum = 0.0
.fsum_loop:
    addsd (%rsi,%rdx,8), %xmm0
    inc %rdx
    cmp %rcx, %rdx
    jl .fsum_loop
```

## 练习

### 练习 1: 运行观察
```bash
mingw32-make run
# pi * 2.0 + 1.0 = 7.283185
# sum(1.1,2.2,3.3,4.4,5.5) = 16.50
# pi > 2.0? yes
```

### 练习 2: 浮点转整数
用 `cvttsd2si` 把 7.28 转成整数，打印观察截断行为

### 练习 3: 观察浮点误差
把 sum 的 `%.2f` 改成 `%.17g`，看看 16.50 背后的真实位模式

### 练习 4: 删掉 movq 会怎样
把 `movq %xmm0, %rdx` 注释掉再运行——亲眼看看 Windows 的坑

## 验证

```bash
mingw32-make build && mingw32-make test
```

## 练习参考答案

> ⚠️ 剧透警告：先自己动手，再看答案！

### 练习 2
```asm
movsd val1(%rip), %xmm0
cvttsd2si %xmm0, %rcx    # rcx = 7 (截断, 不是四舍五入)
lea fmt_i(%rip), %rcx    # "as int: %lld\n"
mov %rcx, ...            # 注意 rcx 现在是新参数! 先挪: 
```
完整写法（先装好 fmt 再转）：
```asm
lea fmt_i(%rip), %rcx
movsd val1(%rip), %xmm0
cvttsd2si %xmm0, %rdx    # 直接转到第 2 参数寄存器
xor %eax, %eax
sub $32, %rsp
call printf
add $32, %rsp
```

### 练习 3
`%.17g` 输出 `16.5` 或 `16.500000000000000`（取决于求和顺序）。
二进制无法精确表示 1.1/2.2 这类十进制小数，累加顺序不同误差不同——
这就是浮点比较要用容差而不是 `==` 的原因。

### 练习 4
输出变成 `pi * 2.0 + 1.0 = 0.000000`。
因为 printf 从 RDX 的 home 槽位读 8 字节，那里是残留垃圾/零。
（打印偶数地址的垃圾也可能是随机小数，总之不是 7.283185）
