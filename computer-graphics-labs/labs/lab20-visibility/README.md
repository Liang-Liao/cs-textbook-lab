# lab20-visibility

对应书中第 20 章：可见性（画家算法失败 vs z-buffer）。

## 学习目标

- 深度缓冲正确处理任意三角形顺序
- 画家算法在相互穿插时失败
- 深度测试可视化（深度灰度图）

## 编译运行

```powershell
mingw32-make -C labs/lab20-visibility run
```

输出：

- `out/lab20_zbuffer.ppm` — z-buffer 结果
- `out/lab20_painter.ppm` — 固定绘制顺序（会错误）
- `out/lab20_depth.ppm` — 深度可视化

## 自检

```powershell
mingw32-make -C labs/lab20-visibility test
```
