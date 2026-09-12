# 第 26 章：最大流（Maximum Flow）

对应《算法导论》第三版第 26 章。实现 Edmonds-Karp（BFS 增广路径）。

## 本章算法

| 算法 | 书中节号 | 源文件 | 复杂度 |
|------|----------|--------|--------|
| Ford-Fulkerson 方法 + BFS 增广 | 26.2 | `maxflow.c` | O(V E²)（Edmonds-Karp） |

## 实现说明

- 残留容量矩阵；每轮 BFS 找最短增广路径并沿路径推送瓶颈容量。
- 运行结束后 `fg_flow_on(u,v)` 用反向残留边读出流量。
- 书中图 26.1 最大流为 **23**。

## 构建与测试

```powershell
mingw32-make ch26
mingw32-make test-ch26
.\build\ch26_maxflow\demo_maxflow.exe
```

## 阅读建议

1. 增广路径、残留网络、割与最大流最小割定理。
2. 为何 Edmonds-Karp 用 BFS 保证多项式时间。
3. 与 Dinic / 预流推进的对比（可作扩展）。
