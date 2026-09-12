# 第 5 章：概率分析与随机算法（Probabilistic Analysis and Randomized Algorithms）

对应《算法导论》第三版第 5 章。实现雇佣问题与均匀随机排列。

## 本章算法

| 算法 | 书中节号 | 源文件 | 要点 |
|------|----------|--------|------|
| 雇佣问题 | 5.1 | `hiring.c` | 按给定顺序面试，更优则雇佣 |
| 随机雇佣 | 5.1 | `hiring.c` | 先随机排列再雇佣 |
| RANDOMIZE-IN-PLACE | 5.3 | `random.c` | Fisher-Yates，原地均匀置换 |

## 理论对照

- 在互不相同的随机排名下，雇佣次数的期望为调和数  
  \(E[X] = H_n = \sum_{i=1}^{n} 1/i = \Theta(\lg n)\)。
- 测试用 Monte Carlo（n=32，2 万次）验证样本均值接近 \(H_{32}\)。

## 构建与测试

```powershell
mingw32-make ch05
mingw32-make test-ch05
.\build\ch05_probabilistic\demo_probabilistic.exe
```

## 阅读建议

1. 先读 5.2 指示器随机变量：\(X_i\) 指示「第 i 位被雇佣」，则 \(E[X]=\sum E[X_i]\)。
2. 为什么随机算法只保证**期望**最坏输入，而不是确定性最坏界？
3. 练习：5.3-2 证明 RANDOMIZE-IN-PLACE 产生均匀分布。
