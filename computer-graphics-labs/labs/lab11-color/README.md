# lab11-color

对应书中第 11 章：颜色。

## 学习目标

- 线性光 vs 显示编码（sRGB）
- 同一场景不同编码的视觉差异
- 简单 RGB 强度叠加与加色混合

## 范围与简化（明确未实现）

- **色域图（CIE 图）与 HSV/YCbCr 等颜色空间转换未实现**；本 lab 聚焦「线性 vs 显示编码」这一条主线。

## 编译运行

```powershell
mingw32-make -C labs/lab11-color run
```

输出 `out/lab11_color.ppm`：上排线性直写、下排 sRGB 编码的同色渐变。

## 自检

```powershell
mingw32-make -C labs/lab11-color test
```
