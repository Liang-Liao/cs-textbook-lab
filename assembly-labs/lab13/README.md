# Lab 13: 性能优化

## 目标
- 理解汇编层面的性能优化
- 学会循环展开
- 用 QueryPerformanceCounter 做真实计时

## 真实计时：Win32 API

```asm
# QueryPerformanceCounter(RCX = 指向 8 字节计数的指针)
lea t_start(%rip), %rcx
call QueryPerformanceCounter    # kernel32, gcc 默认链接

# ... 被测代码 ...

lea t_end(%rip), %rcx
call QueryPerformanceCounter

# 换算微秒: (t_end - t_start) * 1_000_000 / QueryPerformanceFrequency
elapsed_us:
    mov t_end(%rip), %rax
    sub t_start(%rip), %rax
    imul $1000000, %rax         # 先乘后除, 保持精度
    xor %edx, %edx
    div freq(%rip)
    ret
```

这是 Windows 下最精确的计时方式（纳秒级分辨率），比 `time` 命令靠谱得多。

## 数据放 .bss，不占文件体积

```asm
    .comm big_array, 8000000, 64   # 100 万个 .quad，未初始化
```
放在 .comm/.bss 里，可执行文件只有几 KB；如果用 `.rept` 写在 .data 里，
文件会凭空大 8MB。运行时先填充（顺便暖缓存，计时更稳定）。

## 循环展开 (AT&T)

```asm
# 原始: 每次处理 1 个元素
.normal_loop:
    add (%rcx,%rdx,8), %rax
    inc %rdx
    cmp $array_len, %rdx
    jl .normal_loop

# 展开 4x: 每次处理 4 个元素
.unrolled_loop:
    add (%rcx,%rdx,8), %rax
    add 8(%rcx,%rdx,8), %rax
    add 16(%rcx,%rdx,8), %rax
    add 24(%rcx,%rdx,8), %rax
    add $4, %rdx
    cmp $array_len, %rdx
    jl .unrolled_loop
```

## 优化技巧

| 技巧 | 说明 |
|------|------|
| 循环展开 | 减少分支/计数开销，增加指令级并行 |
| 顺序访问 | 缓存友好（空间局部性） |
| .comm 数据 | 不撑大可执行文件 |
| 暖缓存 | 正式计时前先跑一遍填充 |

## 练习

### 练习 1: 运行观察（每次计时不完全相同，正常）
```bash
mingw32-make run
# Normal sum = 1000000, time = 94 us
# Unrolled sum = 1000000, time = 36 us
```

### 练习 2: 多次取最小值
计时受调度干扰，把每个求和跑 10 次取最小值再对比

### 练习 3: 8 路展开
写 sum_unrolled8，看收益是否还有 4 路那么大

### 练习 4: 瓶颈分析
展开到 8 路后为什么收益递减？（提示：加法依赖链 + 每 CPU 周期
可执行的 load/add 数量是有限的）

## 验证

```bash
mingw32-make build && mingw32-make test
# test 会按格式校验输出 (计时不固定, 不能逐字比对)
```

## 练习参考答案

> ⚠️ 剧透警告：先自己动手，再看答案！

### 练习 2
把 sum + 计时包成 loop，外层 ecx 从 10 往下数，每次比较 elapsed_us
取较小者存 r12。伪代码：`best = min(best, elapsed_us())`。

### 练习 3
```asm
sum_unrolled8:
    lea big_array(%rip), %rcx
    xor %eax, %eax
    xor %edx, %edx
.loop:
    add   (%rcx,%rdx,8), %rax
    add  8(%rcx,%rdx,8), %rax
    add 16(%rcx,%rdx,8), %rax
    add 24(%rcx,%rdx,8), %rax
    add 32(%rcx,%rdx,8), %rax
    add 40(%rcx,%rdx,8), %rax
    add 48(%rcx,%rdx,8), %rax
    add 56(%rcx,%rdx,8), %rax
    add $8, %rdx
    cmp $array_len, %rdx
    jl .loop
    ret
```

### 练习 4
这些 add 全部累加到同一个 RAX——每条 add 都要等上一条的 RAX 结果
（**依赖链**）。CPU 的乱序引擎会自动重排，但每周期最多 1-2 个 load +
有限个 ALU 端口，8 路展开后指令窗口装满，瓶颈从"循环开销"变成
"每周期最多 1 次 64 位 load"。想再快就得用**多累加器**（rax+rdx+r8...
分别累加再合并）或 SIMD（lab12 的思路）——这正是现代编译器自动
向量化的原因。
