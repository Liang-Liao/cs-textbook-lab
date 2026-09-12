# 第 34 章：NP 完全性（NP-Completeness）

对应《算法导论》第三版第 34 章。实现几个经典 NP 判定问题的指数时间判定器。

## 本章内容

| 问题 | 书中节号 | 源文件 |
|------|----------|--------|
| VERTEX-COVER 判定 | 34.5 | `np_complete.c` |
| INDEPENDENT-SET 判定 | 34.5 | `np_complete.c` |
| CLIQUE 判定 | 34.5 | `np_complete.c` |
| 3-SAT 判定（小规模） | 34.3 | `np_complete.c` |
| 3-SAT → CLIQUE 归约 | 34.5 | `np_complete.c` |

## 实现说明

- 指数枚举（子集掩码），仅适合 `n ≤ 20` 的教学实例；过大返回 `-1`。
- 3-CNF 文字编码：`+(v+1)` / `-(v+1)`，变量 0-based。
- 3-SAT→CLIQUE 归约（定理 34.10）：按书中构造生成归约图，可满足 ⇔ 有大小为 m 的团；测试用随机公式对拍 `sat3_decision` 与 `clique_decision`。
- 测试演示：`IS ≥ k` 与 `VC ≤ n-k` 在同一图上的一致性（标准归约关系）。

## 构建与测试

```powershell
mingw32-make ch34
mingw32-make test-ch34
.\build\ch34_np_completeness\demo_np.exe
```

## 阅读建议

1. 判定问题 vs 最优化问题；P、NP、NPC 的关系。
2. 3-SAT → Clique / Vertex Cover 的多项式归约（书中 34.5）。
3. 为何此处不做高效求解——NPC 无已知多项式算法（除非 P=NP）。
