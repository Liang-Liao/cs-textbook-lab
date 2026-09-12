# 第 33 章：计算几何（Computational Geometry）

对应《算法导论》第三版第 33 章。实现线段相交与凸包（Jarvis march）。

## 本章算法

| 算法 | 书中节号 | 源文件 | 复杂度 |
|------|----------|--------|--------|
| 叉积方向 / ON-SEGMENT | 33.1 | `geometry.c` | O(1) |
| SEGMENTS-INTERSECT | 33.1 | `geometry.c` | O(1) |
| 凸包 Jarvis march | 33.3 | `geometry.c` | O(nh) |

## 实现说明

- 叉积判断左转/右转；相交含共线重叠与端点接触（eps=1e-12）。
- Jarvis 从最左下点出发，按极角 gift-wrap；共线时取更远点。

## 构建与测试

```powershell
mingw32-make ch33
mingw32-make test-ch33
.\build\ch33_computational_geometry\demo_geometry.exe
```

## 阅读建议

1. 为何 `segments_intersect` 要单独处理共线情形？
2. 凸包输出为逆时针；与 Graham scan / Chan 算法对比。
3. 扩展：多边形点定位、最近点对（书中后续节）。
