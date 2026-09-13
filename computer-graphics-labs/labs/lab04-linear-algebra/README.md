# lab04-linear-algebra

对应书中第 4 章：线性代数与向量几何。

## 学习目标

- 点积 = 投影长度、叉积 = 法向/面积
- 标准正交基与坐标变换
- 用数值自检巩固恒等式

## 编译运行

```powershell
mingw32-make -C labs/lab04-linear-algebra run
```

输出 `out/lab04_vectors.ppm`：二维向量场（每点写单位方向），叠加基向量示意颜色。

## 自检

```powershell
mingw32-make -C labs/lab04-linear-algebra test
```

覆盖：点积正交、叉积方向、正交投影重构、反射公式。
