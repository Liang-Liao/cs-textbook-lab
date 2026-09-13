# lab01-introduction

对应书中第 1 章：图形学管线与「图像即采样函数」的直觉。

## 学习目标

- 把连续函数 \(C(u,v)\) 离散采样成像素
- 观察频率、相位、径向函数如何变成图像
- 理解离线渲染的输入（场景/参数）与输出（像素网格）

## 编译运行

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
mingw32-make -C labs/lab01-introduction run
```

输出：`out/lab01_patterns.ppm`（四格拼贴：棋盘、同心圆、正弦干涉、径向渐变）

## 建议改动

- 改 `g_checker` 棋盘格频率，观察混叠（锯齿）
- 把 `sample` 换成你自己写的解析函数

## 自检

```powershell
mingw32-make -C labs/lab01-introduction test
```
