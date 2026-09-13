# 应用密码学实验室

配合 Bruce Schneier《应用密码学：协议、算法与 C 源程序（原书第 2 版）》**边读边实现**。

- 设计约定、全书 lab 地图、任务规划：见 [DESIGN.md](DESIGN.md)
- 建议阅读顺序：见 [docs/ROADMAP.md](docs/ROADMAP.md)

> 教育用途。**不可用于生产安全**。

## 环境

- Windows + MSYS2 **UCRT64**
- `gcc`、`mingw32-make`（顶层脚本另需 MSYS2 的 `sh`，默认路径见根 `Makefile` 中 `MSYS_SH`）

```powershell
# 确认工具
gcc --version
mingw32-make --version
```

## 快速开始

```powershell
# 构建 common
mingw32-make -C common

# 只学一章（推荐）
cd labs/ch12_des
mingw32-make test

# 顶层一键全量回归
cd ..\..
mingw32-make test
```

`mingw32-make test` 成功后目录里**只保留可执行文件**（`.o` 会被删掉）。

## 目录结构

```text
common/          共享：测试框架、hex、随机、uint64 模运算/素性
labs/chXX_*/     每章一个独立 lab（实现 + 测试 + Makefile + README）
scripts/         顶层 test/clean 的 sh 脚本
docs/ROADMAP.md  阅读顺序
DESIGN.md        工程设计与全书规划
Makefile         顶层聚合 all/test/clean
```

## 已完成 Lab

| Lab | 书中章节 | 内容 |
|-----|----------|------|
| `labs/ch02_building_blocks` | 第2章 | 模运算、素性、单向哈希原型 |
| `labs/ch03_basic_protocols` | 第3章 | 挑战-响应认证 |
| `labs/ch04_intermediate_protocols` | 第4章 | Shamir 秘密共享、票据、时间戳链 |
| `labs/ch05_advanced_protocols` | 第5章 | 哈希承诺、Fiat-Shamir 风格离散对数 ZK |
| `labs/ch06_esoteric_protocols` | 第6章 | 心理扑克盲洗、同时签约（bit-commit） |
| `labs/ch07_key_length` | 第7章 | 暴力耗时模型、生日界、玩具穷举 |
| `labs/ch08_key_management` | 第8章 | KEK/DEK 层级、过期、吊销、wrap |
| `labs/ch09_block_modes` | 第9章 | ECB/CBC/CFB/OFB/CTR |
| `labs/ch11_math_background` | 第11章 | φ(n)、CRT、AES GF(2^8) |
| `labs/ch12_des` | 第12章 | DES、弱密钥 |
| `labs/ch13_other_block` | 第13章 | FEAL 结构教学版 |
| `labs/ch14_more_block` | 第14章 | Blowfish 结构教学版 |
| `labs/ch15_combine_block` | 第15章 | 双重/三重加密、中间相遇 |
| `labs/ch16_stream` | 第16章 | LFSR、RC4 |
| `labs/ch17_stream_and_rng` | 第17章 | monobit/runs/直方图随机性测试 |
| `labs/ch18_hash` | 第18章 | MD5、SHA-1、SHA-256 |
| `labs/ch19_public_key` | 第19章 | RSA、Diffie-Hellman |
| `labs/ch20_pkcs` | 第20章 | PKCS#7 填充、PBKDF2 |
| `labs/ch21_implementation` | 第21章 | 填充 oracle 攻击 |
| `labs/ch22_examples` | 第22章 | 口令→KDF→加密+MAC 迷你信封 |

概念章（不设 lab）：第 1、10、23 章。第 24–33 章为书中源码，本仓库已重写对应算法并测。

## 单章学习循环

1. 读该 lab 的 `README.md`
2. `mingw32-make test` 看基线全绿
3. 对照书改代码 / 加断点
4. 再 `mingw32-make test`
5. `mingw32-make clean`（可选）

## 约定摘要

- lab 之间不互相 include，只依赖 `common`
- 每个 lab 独立 Makefile，不用 CMake
- 测试与实现放同一目录
- 编译中间文件自动清除
