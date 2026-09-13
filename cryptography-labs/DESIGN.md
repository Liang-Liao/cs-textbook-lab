# DESIGN — 应用密码学实验室

配合 Bruce Schneier《应用密码学：协议、算法与 C 源程序（原书第 2 版）》边读边实现。

> 教育用途。实现服务于理解结构与协议流程，**不可用于生产安全**。

---

## 1. 目标与非目标

**目标**

- 书中**可编程对照**的章节，每章一个独立 lab：算法/协议源码 + 测试 + Makefile + README 同目录。
- 学习者可以 `cd labs/chXX_... && mingw32-make test` 单独调试某一章，互不拖累。
- 用经典测试向量（FIPS / RFC / 公开 KAT）验证正确性。
- 顶层 `mingw32-make test` 做一键全量回归。

**非目标**

- 不追求生产级密码库（无侧信道防护、无常数时间、RSA/DH 用小模数）。
- 不引入 GMP/OpenSSL 等外部密码库（除 Windows 下 `bcrypt` 作系统 CSPRNG）。
- 概念章（政治、选型综述）不做 lab，只在本文档中给出阅读笔记提示。

---

## 2. 目录与命名约定

```text
cryptography-labs/
├── README.md
├── DESIGN.md                  # 本文档
├── Makefile                   # 顶层聚合：all / test / clean
├── common/                    # 唯一共享代码
│   ├── crypto_common.h
│   ├── crypto_common.c
│   └── Makefile               # → libcommon.a（构建后删 .o，保留 .a）
├── labs/
│   └── chNN_topic/            # NN 为书中章节号
│       ├── README.md          # 本章目标、步骤、思考题
│       ├── *.c *.h            # 实现与测试（test_chNN.c）
│       └── Makefile           # 独立构建
├── scripts/
│   ├── run_tests.sh           # 遍历 labs 并执行各 lab 的 test 目标
│   └── clean_all.sh           # 遍历 labs 并执行各 lab 的 clean 目标
└── docs/
    └── ROADMAP.md             # 建议阅读顺序
```

**命名规则**

| 项 | 规则 | 示例 |
|----|------|------|
| lab 目录 | `labs/ch{两位章节}_{主题英文}` | `labs/ch12_des/` |
| 测试主文件 | `test_chNN.c` | `labs/ch12_des/test_ch12.c` |
| 可执行文件 | Windows 下为 `test_chNN.exe` | `test_ch12.exe` |
| 多算法一章 | 同一 lab 内多 `.c/.h`，一个 `test_chNN` 覆盖全部 | `ch18_hash` 内 md5+sha1+sha256 |

**独立性规则**

- lab 之间**禁止** `#include` 或链接对方源码。
- 唯一允许依赖：`../../common`（`libcommon.a`）与系统库（`-lbcrypt`）。
- 若后一章需要前一章原语（如模式需要分组密码）：在本 lab **内嵌**教育版实现，或提供可开关的本地副本；默认路径零跨 lab 依赖。

---

## 3. common 职责（保持薄）

| 提供 | 不提供 |
|------|--------|
| 测试框架（`test_stats_t` / `test_check` / `test_check_eq_hex`） | 任何分组/流/公钥算法本体（DES/MD5/RSA…） |
| hex dump / 转换、`xor_bytes`、`memeq_const` | 协议状态机 |
| `crypto_random_bytes`（Win32 BCrypt） | 文件/网络 I/O |
| `mod_mul_u64` / `mod_pow_u64` / `mod_inv_u64` | 大整数库 |
| `gcd_u64` / `ext_gcd_u64` / `is_prime_u64` / `random_prime_u64` | 生产级素性/参数 |
| `mix64_u64`（协议演示用混合，**非**密码哈希） | |

数论原语放在 common 是为了 ch02/ch19 等多 lab 共用，避免互相 include。

---

## 4. 单 Lab 生命周期

```text
阅读 README.md  →  mingw32-make test   # 编译 + 删 .o + 跑测试
              →  改源码/断点            # mingw32-make / gdb
              →  mingw32-make test     # 再验
              →  mingw32-make clean    # 删可执行与残留中间文件
```

**Makefile 实际约定（MSYS2 + mingw32-make，recipe 走 cmd.exe）**

Windows 下 `mingw32-make` 默认用 `cmd`，且**不会**采用 Makefile 里的 `SHELL := sh`。因此：

- 删除文件用 `del /F /Q ... 2>NUL`（不用 `rm` / `$(RM)`）
- 运行测试用 `.\$(TARGET).exe`（不用 `./$(TARGET)`）
- `test` 依赖 `all`，以便链接后先删 `.o` 再跑测试

```make
CC       ?= gcc
CFLAGS   ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -g
CPPFLAGS  = -I../../common
LDLIBS    = -L../../common -lcommon -lbcrypt

TARGET = test_chNN
OBJS   = $(SRCS:.c=.o)

all: $(TARGET)
	-del /F /Q $(OBJS) 2>NUL

test: all
	.\$(TARGET).exe

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

clean:
	-del /F /Q $(TARGET).exe $(OBJS) 2>NUL
```

约定：

1. `all` 结束后 **只保留可执行文件**，主动删除 `.o`。
2. `test` 依赖 `all`，失败非零退出码（便于 CI/顶层聚合）。
3. `clean` 清可执行 + 中间文件。
4. Windows/MSYS2 UCRT64：`gcc`、`mingw32-make`、`-lbcrypt`。

---

## 5. 全书章节 → Lab 规划

图例：**✅ 已实现** · **🔶 计划实现** · **📖 概念章（不设 lab，仅笔记）**

### Part I 密码协议

| 章 | 标题 | 类型 | Lab | 实现要点 | 状态 |
|----|------|------|-----|----------|------|
| 1 | 基础 | 📖 | — | 术语、协议 vs 算法、攻击分类 | 概念 |
| 2 | 协议构件 | ✅ | `ch02_building_blocks` | 模运算/素性（common）+ Davies-Meyer 单向哈希原型（lab 内） | ✅ |
| 3 | 基础协议 | ✅ | `ch03_basic_protocols` | 共享秘密挑战-响应认证 | ✅ |
| 4 | 中间协议 | ✅ | `ch04_intermediate_protocols` | Shamir 秘密共享、简化票据、时间戳链 | ✅ |
| 5 | 高级协议 | ✅ | `ch05_advanced_protocols` | 哈希承诺、Fiat-Shamir 风格离散对数 ZK（玩具） | ✅ |
| 6 | 异常协议 | ✅ | `ch06_esoteric_protocols` | 心理扑克（可交换锁盲洗）、同时签约（bit-commit） | ✅ |

### Part II 密码技术

| 章 | 标题 | 类型 | Lab | 实现要点 | 状态 |
|----|------|------|-----|----------|------|
| 7 | 密钥长度 | ✅ | `ch07_key_length` | 暴力搜索耗时模型、生日界计算、穷举 demo（限定位宽） | ✅ |
| 8 | 密钥管理 | ✅ | `ch08_key_management` | KEK/DEK 层级、过期/吊销、玩具 wrap/unwrap | ✅ |
| 9 | 算法类型与模式 | ✅ | `ch09_block_modes` | ECB/CBC/CFB-8/OFB/CTR；自带 toy Feistel，不依赖 DES lab | ✅ |
| 10 | 使用算法 | 📖 | — | 如何选算法、模式组合、实战清单 | 概念 |

### Part III 密码算法

| 章 | 标题 | 类型 | Lab | 实现要点 | 状态 |
|----|------|------|-----|----------|------|
| 11 | 数学背景 | ✅ | `ch11_math_background` | φ(n)、CRT、AES GF(2^8) 与 clmul | ✅ |
| 12 | DES | ✅ | `ch12_des` | IP/FP、Feistel、S 盒、密钥编排、弱/半弱密钥、经典向量 | ✅ |
| 13 | 其它分组算法 | ✅ | `ch13_other_block` | FEAL 风格结构教学版 | ✅ |
| 14 | 再其它分组算法 | ✅ | `ch14_more_block` | Blowfish 结构教学版（约简 S 盒初值，见 lab README） | ✅ |
| 15 | 组合分组算法 | ✅ | `ch15_combine_block` | 双重/三重 EDE、中间相遇 demo | ✅ |
| 16 | 流密码与伪随机 | ✅ | `ch16_stream` | LFSR 周期与线性、RC4 KSA/PRGA、KAT | ✅ |
| 17 | 其它流密码与真随机 | ✅ | `ch17_stream_and_rng` | monobit/runs/字节直方图随机性测试，对照 CSPRNG 与弱源 | ✅ |
| 18 | 单向哈希函数 | ✅ | `ch18_hash` | MD5、SHA-1、SHA-256；Merkle-Damgård；官方向量 | ✅ |
| 19 | 公钥算法 | ✅ | `ch19_public_key` | RSA（小模数）、DH；教学填充结构 | ✅ |
| 20 | PKCS | ✅ | `ch20_pkcs` | PKCS#7 填充、PBKDF2-HMAC-SHA256 | ✅ |

### Part IV 真实世界

| 章 | 标题 | 类型 | Lab | 实现要点 | 状态 |
|----|------|------|-----|----------|------|
| 21 | 实现细节 | ✅ | `ch21_implementation` | 填充 oracle 攻击 demo | ✅ |
| 22 | 例题与综合方案 | ✅ | `ch22_examples` | 口令→PBKDF2→加密+HMAC 迷你信封 | ✅ |
| 23 | 密码政治 | 📖 | — | 出口管制、密钥托管争论（阅读） | 概念 |

### Part V 源程序

| 章 | 说明 |
|----|------|
| 24–33 | 书中附带 C 源码；本仓库**重写**对应算法并测，不直接拷贝原书代码 |

---

## 6. 分阶段任务规划

| 阶段 | 内容 | 验收 |
|------|------|------|
| **P0 设计** | 本 DESIGN + 根 README + ROADMAP 分工说明 | 文档覆盖全书 lab 地图 |
| **P1 骨架** | `common/Makefile`、顶层 `Makefile`、`labs/` 树 | 顶层 `make` 可遍历空/已有 lab |
| **P2 迁移** | 7 个已有实现迁入 `labs/chXX_*`，实现+测试同目录，独立 Makefile | 每个 lab 单独 `mingw32-make test` 通过 |
| **P3 清理** | 删除 `CMakeLists.txt`、`build/`、旧 `chXX_*/`、`tests/` | 树内无 CMake；顶层 `mingw32-make test` 全绿 |
| **P4 增量 lab** | 已全部落地：ch04 → ch11 → ch15 → ch13/14 → ch20 → ch21 → ch05/06/07/08/17/22 | 每个 lab 独立 `mingw32-make test` + README |

**P4 状态：计划中的 lab 已全部完成（2026-09）。** 后续可选增强见各 lab README「动手实验」。

---

## 7. Lab README 统一模板

每个 `labs/chXX_*/README.md` 建议包含：

1. **书中位置**：第 N 章标题与阅读重点
2. **实验目标**：跑通后你能解释/手推什么
3. **文件清单**：各 `.c/.h` 对应书中哪一节
4. **构建与测试**
5. **动手实验**：改哪个变量 → 期望看到什么（2–3 条）
6. **思考题**：攻击面、与现代替代算法的关系
7. **与现代对照**：书出版后的重大变化

---

## 8. 工具链

| 项 | 约定 |
|----|------|
| OS | Windows 10/11 |
| 环境 | MSYS2 **UCRT64** |
| 编译器 | `gcc`（C11） |
| 构建 | **`mingw32-make`**（MSYS2 UCRT64）；**不用 CMake** |
| 随机 | `BCryptGenRandom`（`-lbcrypt`） |
| 调试 | `-g` 默认开启；可用 `gdb ./test_chNN.exe` |
| POSIX 脚本 | 顶层 `test`/`clean` 经 `scripts/*.sh`，由 `MSYS_SH`（默认取 PATH 中的 `sh`，可用 `make test MSYS_SH=<路径>` 指定）执行 |

**顶层 Makefile 职责**：发现 `labs/ch*`、先构建 `common`、`all` 时递归各 lab；`test`/`clean` 委托给 `scripts/run_tests.sh` 与 `scripts/clean_all.sh`（在 Windows cmd 下避免 Makefile 内写复杂 shell 循环）。

---

## 9. 实现质量门禁（每个 lab 合并前）

- [ ] `mingw32-make test` 退出码 0
- [ ] 至少一组**已知公开向量**或自洽回环 + 不变量
- [ ] 无跨 lab include
- [ ] `mingw32-make` 后目录内无 `.o`
- [ ] README 可被未读过源码的人按步骤跑通

---

## 10. 变更记录

| 日期 | 变更 |
|------|------|
| 初版 | 从 CMake 单体工程重构为「common + 独立 lab Makefile」；补全全书实验地图 |
| 修订 | 文档与实现对齐：Windows cmd 风格 Makefile、`scripts/`、common 实际 API、ch03 范围收窄为挑战-响应 |
| 2026-09-12 | 补齐剩余 lab：ch05/06/07/08/17/22；修复 ch07 玩具加密与年数断言、ch05 ZK 缓冲、ch08 ID 比较 |
| 2026-09-12 | 全库审计修复：ch06 承诺长度分帧（拼接歧义）、ch08 ID 超长拒绝+查重（别名）、ch19 DH g=7（真本原根）、ch17 runs 检验改为 FIPS 140-2 界、ch11 euler_phi 溢出；common mod_inv 全域重写/mod_pow 防御/random_prime 小区间；ch03/ch05 RNG 失败显式传错；ch09 长度校验+CFB8 原地安全；ch14 空密钥拒绝；ch07 哨兵统一；注释订正（des/lfsr/oracle 等）；补强 ch16/18/20/21/22 测试 |
