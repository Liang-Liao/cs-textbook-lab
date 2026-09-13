---
feature: audit-labs-docs
status: delivered
updated: 2026-09-12
branch: master
commits: c44d44c..working-tree
---

# Audit Labs and Docs

## Report

**What was built** — 对全部 20 个 lab 与 `common`、顶层文档做了独立审计，并修复 CRITICAL/主要 MAJOR。最严重的两处逻辑缺陷：ch05 的 Fiat-Shamir ZK 验证未重算挑战，可伪造知识证明；ch21 的 Vaudenay 攻击字节序错误且非末块直接用密钥解密，测试被掩盖。另有 common 模乘溢出、素数采样越界、ch16 错误 taps 文档与空断言、ch15 MITM 实为 \(O(n^2)\) 等。修复后 `mingw32-make test` 20 lab 全绿；文档与实现对齐（含 ch05 承诺长度前缀）。

**Verification** — 仓库根 `mingw32-make test`：`All labs passed (20).` 修复后 ch05 7/7（含伪造拒绝）、ch06 14/14（含 SHA KAT）、ch16 6/6（真周期 65535）、ch21 5/5（完整明文恢复）。独立 review 确认 5 项 CRITICAL 均已落地；唯一残留 major（ch05 头文件/README 仍写 `H(r‖m)`）已当场改掉并复测 PASS。

**Journey log** —
1. 「测试全绿」≠「逻辑正确」：ch21 用密钥作弊恢复前缀，末块从不比对，攻击体无意义。
2. Fiat-Shamir 必须在 verify 侧重算 `c`，否则校验方程可被任意 \(z,c\) 满足。
3. 文档写 `H(r‖m)` 而代码做域分离时，必须同步 header/README，否则 review 仍会判不对齐。
4. 审计应分 lab 组并行；CRITICAL 集中在协议 ZK 与 padding oracle 两类「看起来能过」的 demo。
5. 未全清的 MINOR（如 `ticket_open` 恒 0、弱密钥表含半弱密钥）已记录，不阻塞教学主线。

## [S1] Problem

用户要求全库复查：遗漏实现、实现缺陷、文档与实现是否对应。上一轮交付 ch05/06/07/08/17/22 后宣称 20 lab 完成，但缺少独立一致性审计。

## [S2] Design

审计全部 `labs/ch*`、`common/`、根文档与各 lab README。

**CRITICAL（已修）**

| 问题 | 修复 |
|------|------|
| ch05 `zk_verify` 不重算 Fiat-Shamir 挑战，可伪造证明 | `fs_challenge` 在 prove/verify 共用；verify 比较 `expect_c`；加伪造证明拒绝测试 |
| ch05 `commit` 无长度前缀，`H(r‖m)` 拼接歧义 | 哈希 `rlen‖r‖mlen‖m`；header/README 同步 |
| ch21 last-block 恢复字节序错误 + 非末块用密钥作弊 | 全 CBC 仅经 `oracle_padding_ok` 恢复；`pos=0` 二次验证消假阳性；测试比对完整明文 |
| common `mod_mul_u64` 大模数溢出 | `__uint128_t`（gcc）/ 溢出安全加减回退 |
| common `random_prime_u64` 可能返回 `hi` | 钳制到 `[lo,hi)` 内奇数 |

**MAJOR（已修）**

- ch05/ch06 增加 SHA-256 FIPS `"abc"` KAT
- ch16：LFSR 文档 tap 改为 `0x002D`；周期测试逐步测满 \(2^{16}-1\)；去掉恒真断言；RC4 空 key 安全
- ch15：MITM 改为排序+二分；测试名去掉误称 EDE
- ch19：`n=p*q` 溢出拒绝；PKCS 注释与实现对齐；`need>=1`
- ch22：salt 失败即 `env_seal` 失败
- ch14 README `S_INIT` 笔误、`stddef.h`；ch17 README 比特交替表述

**MINOR（记录未全修）**：ch03 `wmf_ticket_t` 无函数、ch04 `ticket_open` 恒 0、ch07 返回值、ch08 `now` 未用、ch12 弱/半弱密钥未分列、ch09 非对齐长度未测等。教学范围可接受或低风险。

**文档对齐结论**：根 README / DESIGN §5 / ROADMAP 与 20 个 lab 一致；本轮修正实现与 README 冲突点（ch15 复杂度、ch16 taps、ch14 S 盒、ch17 实验、ch19 padding、ch21 算法、ch05 承诺格式）。

## [S3] Out of Scope

- 不重写已 CLEAN 的 lab（ch02/13/18 等）
- 不把全部 MINOR 一次清零
- 不引入外部密码库

## Tasks

- [x] T1: 写 audit feature 文档 — acceptance: 本文件存在 (covers: S1)
- [x] T2: 审计 ch02–ch08 — acceptance: 发现列表 (covers: S2)
- [x] T3: 审计 ch09–ch17 — acceptance: 发现列表 (covers: S2)
- [x] T4: 审计 ch18–ch22 + 顶层文档 — acceptance: 发现列表 (covers: S2)
- [x] T5: 修复 critical/major — acceptance: 修复后全量 test 绿 (covers: S2; depends: T2,T3,T4)
- [x] T6: Review — acceptance: 独立 reviewer 确认修复与文档对齐 (covers: S1,S2; depends: T5)
