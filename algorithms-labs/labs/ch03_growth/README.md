# 第 3 章：函数的增长（Growth of Functions）

对应《算法导论》第三版第 3 章。本 lab 用数值方式感受常见复杂度函数的相对增长速度，并提供渐近比较的粗粒度分类工具。

## 本章内容

| 内容 | 书中节号 | 源文件 |
|------|----------|--------|
| lg n、n lg n、n^k、c^n、lg(n!)、H_n | 3.1–3.2 | `growth.c` |
| 增长趋势比较（比值是否趋于 0 / 常数 / ∞） | 3.1 | `growth_compare` |
| 数值对照表与 Harmonic / Stirling 直观 | 3.2 | `demo_growth.c` |

## 学习重点

1. 渐近上界 Ο、下界 Ω、紧界 Θ 的**定义**（书中 3.1）比记公式更重要。
2. `n lg n = Θ(lg n!)`、`H_n = Θ(lg n)` 会反复出现在后续平均复杂度分析里。
3. `growth_compare` 只是**教学用**的比值趋势启发式，不是形式证明。

## 构建与测试

```powershell
mingw32-make ch03
mingw32-make test-ch03
.\build\ch03_growth\demo_growth.exe
```

## 阅读建议

1. 对照书中表 3.1，观察 demo 打印的 `2^n` 如何迅速超过多项式。
2. 练习：3.2-3（证明多项式界等价性）是笔头题；可把你的证明要点写在本 README。
3. 练习：用 `growth_lg_factorial` 验证 Stirling 近似的相对误差。
