---
feature: finish-remaining-labs
status: delivered
updated: 2026-09-12
branch: master
commits: 56ed710..working-tree
---

# Finish Remaining Labs

## Report

**What was built** — 补齐《应用密码学》剩余 6 个 lab 并全量回归通过。`ch05`/`ch07`/`ch08`/`ch17` 在既有骨架上修缺陷并补 README：ch05 将 Fiat-Shamir 证明的哈希输出缓冲从 8 字节扩到 32；ch07 补 `math.h`、把玩具加密改为重复密钥字节、修正 128-bit 年数量级断言；ch08 用零填充定长缓冲比较密钥 ID，消除短 C 字符串越界读；ch17 保持 monobit/runs/直方图测试。新建 `ch06_esoteric_protocols`（可交换锁心理扑克 + bit-commit 同时签约）与 `ch22_examples`（口令→PBKDF2→encrypt-then-MAC 迷你信封，错口令/篡改必拒）。根 README、DESIGN §5、ROADMAP 与 6 个 lab 的 README 同步为「20 lab 全部完成」。

**Verification** — 仓库根 `mingw32-make test`：`All labs passed (20).` 单测计数：ch05 4、ch06 13、ch07 4、ch08 5、ch17 5、ch22 8，其余已提交 lab 亦全绿。Review 无 critical；随后又修了 ch22 HMAC OOM 仍返回成功、envelope 头注释、ch06 未用 seed 参数，ch06/ch22 复测 PASS。

**Journey log** —
1. 未跟踪 lab 里有 4 个近完成骨架、2 个只有 sha256 拷贝——先验证再补实现，比重写更省。
2. ch07 玩具 `enc8` 原先只把密钥低字节加到第 0 字节，与测试「全字节 +0x10」预期不一致；改为对全部 8 字节重复同一密钥字节。
3. ch05 `sha256(buf,32,ch)` 的 `ch[8]` 是真实栈越界，不只是编译器警告。
4. 用户明确选择在主 worktree `master` 继续，未建 `.worktrees`。
5. 文档状态与实现同步（README/DESIGN/ROADMAP）是 lab 收口的最后一环，否则「计划中」会一直过期。

## [S1] Problem

仓库已落地 14 个已提交 lab（ch02–04、09、11–16、18–21）。剩余 6 个目录处于未跟踪状态：

| Lab | 现状 | 缺口 |
|-----|------|------|
| `ch05_advanced_protocols` | 源码+测试+Makefile 齐 | 未验证；README 过薄 |
| `ch07_key_length` | 源码+测试+Makefile 齐 | 未验证；README 过薄 |
| `ch08_key_management` | 源码+测试+Makefile 齐 | 未验证；`km_find`/`km_revoke` 用 `memcmp(..., KM_ID_LEN)` 可能越界读短 C 字符串 |
| `ch17_stream_and_rng` | 源码+测试+Makefile 齐 | 未验证；README 过薄 |
| `ch06_esoteric_protocols` | 仅 `sha256.c/.h` | 无实现/测试/Makefile/README |
| `ch22_examples` | 仅 `sha256.c/.h` | 无实现/测试/Makefile/README |

`DESIGN.md` / `docs/ROADMAP.md` / 根 `README.md` 尚未把这些 lab 标为已完成。

## [S2] Design

沿用仓库既有约定：每个 lab 独立 Makefile（MSYS2 UCRT64 + `mingw32-make`，cmd recipe）、只依赖 `../../common`、实现与测试同目录、`test` 退出码 0 为门禁。

**ch05**：哈希承诺 `c=H(r‖m)` + Fiat-Shamir 风格离散对数 ZK（`g^z ≡ A·X^c (mod p)`）。不扩展图同构/数字现金——现有范围已覆盖第 5 章「高级协议」教学点。

**ch07**：暴力搜索年数 `2^bits / rps`、生日界 `2^{b/2}`、8-bit 玩具穷举（重复密钥字节加法）。

**ch08**：KEK/DEK 存储、过期、吊销、XOR+mix64 wrap/unwrap。ID 比较先拷到定长零填充缓冲再 `memcmp`。

**ch17**：monobit 偏置、runs、字节直方图偏差；对照 BCrypt CSPRNG 与全零弱源。

**ch06**：
- 心理扑克：可交换加锁（mod 52）+ 双方 Fisher-Yates，最终牌堆仍是 0..51 排列。
- 同时签约：双方 SHA-256 bit-commit，再交换 opening；单方违约时另一方可证明对方仍被承诺绑定。
- 自带本地 `sha256`（零跨 lab 依赖）。

**ch22**：迷你信封
- 口令 + salt → PBKDF2-HMAC-SHA256 派生 64 字节（enc‖mac key）
- PKCS#7(8) + `mix64` 密钥流 XOR，HMAC-SHA256 over `salt‖ct`（encrypt-then-MAC）
- 正确口令可解密；篡改密文或错误口令必须失败

**文档**：各 lab README 按 DESIGN §7 模板补全；根 README 已完成表、ROADMAP 已完成表、DESIGN §5 状态列同步。

## [S3] Out of Scope

- 不改 `common` 公共 API（除非验证证明必须，且单独立项）
- 不做生产密码学（侧信道、常数时间、真实 AES/RSA）
- 不实现 ch23 政治章（概念章）
- 不在本轮把未跟踪源码从主 worktree 迁走（用户明确选择留在 master）

## Tasks

- [x] T1: 写 feature 文档 — acceptance: 本文件存在且 status=designed (covers: S1)
- [x] T2: 修复 ch08 ID 比较 — acceptance: `km_find`/`km_revoke` 不越界；测试仍过 (covers: S2)
- [x] T3: 验证 ch05/ch07/ch08/ch17 — acceptance: 各目录 `mingw32-make test` 退出 0 (covers: S2; depends: T2)
- [x] T4: 实现 ch06 lab — acceptance: `labs/ch06_esoteric_protocols` 有实现+测试+Makefile+README，`mingw32-make test` 退出 0 (covers: S2)
- [x] T5: 实现 ch22 lab — acceptance: `labs/ch22_examples` 同上，且篡改/错口令被拒 (covers: S2)
- [x] T6: 同步根 README / DESIGN / ROADMAP 与各 lab README — acceptance: 已完成表覆盖全部 20 lab，状态与磁盘一致 (covers: S1,S2)
- [x] T7: 顶层全量回归 — acceptance: 仓库根 `mingw32-make test` 退出 0 (covers: S2)
