# lab22-rasterization

对应书中第 22 章：光栅化（边方程、重心插值、z-buffer）。

## 学习目标

- 三角形边方程判定覆盖
- 重心坐标插值颜色与深度
- 软件 z-buffer 正确处理遮挡

## 编译运行

```powershell
mingw32-make -C labs/lab22-rasterization run
```

输出：`out/lab22_raster.ppm` — 彩色三角形网格 + 深度测试，包含前后重叠面。

## 自检

```powershell
mingw32-make -C labs/lab22-rasterization test
```

验证：点是否在三角形内、重心坐标和为 1、z-buffer 近者覆盖远者。

## 与 lab03 对照

lab03 是光线求交（解析球 + 平面），lab22 是三角形光栅化 + z-buffer。
两者场景刻意不同：lab22 用网格高度场三角形展示边方程/重心插值；若把 lab03 的球改成三角网格再走 lab22 管线，可在相同相机下对比“求交 vs 覆盖采样”的实现路径差异。
