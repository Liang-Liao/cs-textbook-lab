# FoCG 第 5 版 Lab 进度

| Lab | 章 | 主题 | 状态 |
|-----|----|------|------|
| lab00 | 前置 | 工具链烟雾测试 | done |
| lab01 | 1 | Introduction | done |
| lab02 | 2 | Raster Images | done |
| lab03 | 3 | Ray Tracing | done |
| lab04 | 4 | Linear Algebra | done |
| lab05 | 5 | Transformation Matrices | done |
| lab06 | 6 | Viewing | done |
| lab07 | 7 | Texture Mapping | done |
| lab08 | 8 | Data Structures | done |
| lab09 | 9 | Shading | done |
| lab10 | 10 | Rays and More | done |
| lab11 | 11 | Color | done |
| lab12 | 12 | Visual Perception | done (optional) |
| lab13 | 13 | More Ray Tracing | done |
| lab14 | 14 | Sampling | done |
| lab15 | 15 | Curves | done |
| lab16 | 16 | Surfaces | done |
| lab17 | 17 | Light | done |
| lab18 | 18 | Materials | done |
| lab19 | 19 | Wave Optics | done (optional toy) |
| lab20 | 20 | Visibility | done |
| lab21 | 21 | RT Hardware | done (concept dump) |
| lab22 | 22 | Rasterization | done |
| lab23 | 23 | Hardware Features | done |

## 快速开始

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
mingw32-make list
mingw32-make -C labs/lab00-toolchain-smoke run
mingw32-make run LAB=lab03-ray-tracing
```

各 lab 输出在其 `labs/<lab>/out/` 目录。
