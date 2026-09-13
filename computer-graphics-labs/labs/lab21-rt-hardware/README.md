# lab21-rt-hardware

对应书中第 21 章：实时图形硬件（概念章，软件流水线阶段模拟）。

## 学习目标

- 顶点 → 裁剪空间 → NDC → 屏幕坐标
- 图元装配与光栅化阶段输入
- 每阶段中间结果 dump（文本日志）

## 范围说明

前四个阶段（顶点、clip、NDC/透视除法、视口）输出真实数值 dump；图元装配、光栅化、片元三个阶段在日志中是文字占位（指向 lab22 的边方程/重心插值实现），未做数据级 dump。

## 编译运行

```powershell
mingw32-make -C labs/lab21-rt-hardware run
```

输出：`out/lab21_pipeline.txt`

## 自检

```powershell
mingw32-make -C labs/lab21-rt-hardware test
```
