# lab00-toolchain-smoke

前置实验：确认 MSYS2 UCRT64 工具链与 `common` 库可编译运行，并写出第一张 PPM。

## 学习目标

- 确认 `gcc` / `mingw32-make` 可用
- 理解图像 = 二维采样函数：`C(x,y) -> RGB`
- 掌握本仓库统一的图像写出约定（线性 RGB → 可选 sRGB → PPM）

## 编译运行

```powershell
# PowerShell：先把 UCRT64 bin 加入 PATH
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"

mingw32-make -C labs/lab00-toolchain-smoke run
```

默认输出：`out/lab00_gradient.ppm`（sRGB 编码）。

## 预期现象

- 左上偏红，向右变绿，向下变蓝，形成平滑三原色渐变
- 退出码 0

## 建议改动

- 改宽高、改函数 `f(x,y)`
- 用 `cgl_image_write_ppm(..., binary=0)` 输出 P3，用文本 diff 观察

## 自检

```powershell
mingw32-make -C labs/lab00-toolchain-smoke test
```
