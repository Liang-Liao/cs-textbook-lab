# CS Textbook Labs — 计算机经典教材配套实验合集

本仓库收录围绕计算机科学经典教材系统整理的动手实验（Lab）系列。每个子目录是一个
自包含的子项目：自带说明文档、构建脚本与自动化测试，可独立克隆、构建和验证。

## 子项目一览

| 子项目 | 配套教材 | 内容 | 语言 / 构建 | 规模 |
|--------|----------|------|-------------|------|
| [algorithms-labs](algorithms-labs/) | 《算法导论》（CLRS 第 3 版） | 逐章算法实现与测试 | C11 / GNU Make | 32 个实验 |
| [assembly-labs](assembly-labs/) | 汇编语言（王爽）+ CSAPP 等 | x86-64 汇编递进实验 | GAS 汇编 + C / GNU Make | 16 个实验 |
| [compiler-labs](compiler-labs/) | 《编译原理》（龙书 第 2 版） | 逐 lab 构造 MiniC 编译器 | C11 / GNU Make + bash | 13 个实验 |
| [computer-graphics-labs](computer-graphics-labs/) | 《计算机图形学基础》（FoCG 第 5 版） | 软件光线追踪与光栅化实验 | C11 / GNU Make | 24 个实验 |
| [cryptography-labs](cryptography-labs/) | 《应用密码学》（Schneier 第 2 版） | 逐章密码算法与协议实现 | C11 / GNU Make | 20 个实验 |

## 快速开始

各子项目均以 **MSYS2 UCRT64**（gcc + GNU Make）为主要验证环境，Windows / Linux 通用。

```bash
# 算法实验（CLRS）
cd algorithms-labs
make            # 构建全部章节
make test       # 运行全部测试

# 汇编实验（x86-64）
cd assembly-labs
mingw32-make test

# 编译原理实验（龙书）
cd compiler-labs/lab1-lexer
bash tests/run_tests.sh

# 图形学实验（FoCG，离线输出 PPM 图像，无需 GPU）
cd computer-graphics-labs
mingw32-make test                          # 24 个 lab 自检
mingw32-make run LAB=lab03-ray-tracing     # 运行单个 lab 出图

# 密码学实验（应用密码学）
cd cryptography-labs
mingw32-make test
```

各子项目的详细说明、学习路线与实验清单见对应目录下的 `README.md`。

## 环境要求

- gcc（C11），建议 MSYS2 UCRT64 工具链（Windows）或任意 GCC ≥ 11（Linux/macOS）
- GNU Make（Windows 下可用 MSYS2 的 `mingw32-make`）
- compiler-labs 与 cryptography-labs 的顶层测试脚本需要 bash/sh（MSYS2 / Git Bash / WSL 均可）

## License

[MIT](LICENSE)
