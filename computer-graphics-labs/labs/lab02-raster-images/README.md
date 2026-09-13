# lab02-raster-images

对应书中第 2 章：光栅图像、采样与量化。

## 学习目标

- 分辨率降低如何丢失高频细节（混叠）
- 8-bit 量化带来的色带（banding）
- 简单箱式滤波下采样 vs 直接点采样

## 编译运行

```powershell
mingw32-make -C labs/lab02-raster-images run
```

输出 `out/lab02_sampling.ppm`：四格 — 高频棋盘点采样、下采样后放大、8 级量化、16 级量化。

## 建议改动

- 提高棋盘频率直到锯齿严重
- 把量化级数从 8 改到 256，观察色带消失

## 自检

```powershell
mingw32-make -C labs/lab02-raster-images test
```
