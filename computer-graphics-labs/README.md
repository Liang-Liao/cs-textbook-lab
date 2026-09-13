# Computer Graphics Labs

以《Fundamentals of Computer Graphics》第 5 版为主线的逐章可运行 C 实验。

- 渲染范式：纯软件（光线/路径追踪 + 软件光栅化），离线 PPM 输出
- 环境：Windows + MSYS2 UCRT64（gcc / mingw32-make）
- 设计文档：[`docs/compose/spec/fo-cg-labs-curriculum.md`](docs/compose/spec/fo-cg-labs-curriculum.md)
- 进度对照：[`docs/roadmap.md`](docs/roadmap.md)

## 快速开始

```powershell
# 将 UCRT64 加入 PATH（按 MSYS2 实际安装路径调整，下同）
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"

# 列出 labs
mingw32-make list

# 构建并运行单个 lab
mingw32-make -C labs/lab03-ray-tracing run
# 或
mingw32-make run LAB=lab03-ray-tracing

# 全量构建 / 自检 / release
mingw32-make all
mingw32-make test
mingw32-make release

# 自检单个 lab
mingw32-make -C labs/lab03-ray-tracing test
```

顶层 Makefile 使用纯 Make 规则，PowerShell / cmd / MSYS2 bash 均可运行（构建脚本按 `MSYSTEM` 环境变量选择 POSIX 或 cmd 语法的 mkdir/rm；若在非 MSYS shell 中运行且 PATH 上有 `sh.exe`，建议改用 MSYS2 shell）。图像默认写到各 lab 的 `out/*.ppm`。

## 目录结构

```
common/     极薄公共库（向量/矩阵、图像 PPM、RNG）
make/       共享 lab.mk
labs/       各章独立实验
docs/       设计规格与 roadmap
```

## 学习建议

按书章节顺序做 lab，对照 README 中的「学习目标 / 建议改动」动手改参数再出图。lab03 与 lab13/22 是核心管线两翼（追踪 vs 光栅），建议优先吃透。
