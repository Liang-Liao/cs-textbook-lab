# 第 9 章：中位数与顺序统计量（Medians and Order Statistics）

对应《算法导论》第三版第 9 章。实现最小/最大、期望线性选择与最坏线性选择。

## 本章算法

| 算法 | 书中节号 | 源文件 | 复杂度 |
|------|----------|--------|--------|
| MIN-MAX（两次扫描 / 成对比较） | 9.1 | `order_statistics.c` | n−1 或 ≈3n/2 次比较 |
| RANDOMIZED-SELECT | 9.2 | `order_statistics.c` | 期望 Θ(n) |
| SELECT（五数取中） | 9.3 | `order_statistics.c` | 最坏 Θ(n) |

## 实现说明

- 下标 **0-based**：第 `i` 小为 `i=0`（最小）… `i=n-1`（最大）；书中 1-based 的 `i` 需减 1。
- 划分复用 Lomuto；随机化选择依赖本 lab 的 `order_stat_srand`。
- `SELECT` 对每组 5 个做插入排序，中位数收集后递归取「中位数的中位数」再划分。

## 构建与测试

```powershell
mingw32-make ch09
mingw32-make test-ch09
.\build\ch09_order_statistics\demo_order_statistics.exe
```

## 阅读建议

1. 为何「中位数的中位数」能保证划分后两侧至少约 3n/10？
2. 对比 9.2 与 9.3：期望线性 vs 最坏线性的工程权衡。
3. 练习：9.1-2 同时找最大和次大；可在本 lab 扩展。
