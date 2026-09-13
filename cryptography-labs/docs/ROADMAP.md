# 学习路线：《应用密码学》第2版

工程约定与全书 lab 地图见 [../DESIGN.md](../DESIGN.md)。

## 已完成 lab（建议顺序）

1. `labs/ch02_building_blocks` — 模运算、素性、单向函数
2. `labs/ch03_basic_protocols` — 挑战-响应
3. `labs/ch04_intermediate_protocols` — Shamir 秘密共享、票据、时间戳链
4. `labs/ch05_advanced_protocols` — 承诺、Fiat-Shamir ZK
5. `labs/ch06_esoteric_protocols` — 心理扑克、同时签约
6. `labs/ch07_key_length` — 暴力耗时、生日界
7. `labs/ch08_key_management` — KEK/DEK、过期与吊销
8. `labs/ch09_block_modes` — ECB/CBC/CFB/OFB/CTR
9. `labs/ch11_math_background` — φ(n)、CRT、AES 域
10. `labs/ch12_des` — DES
11. `labs/ch13_other_block` — FEAL
12. `labs/ch14_more_block` — Blowfish
13. `labs/ch15_combine_block` — 双重/三重与中间相遇
14. `labs/ch16_stream` — LFSR、RC4
15. `labs/ch17_stream_and_rng` — 随机性测试
16. `labs/ch18_hash` — MD5/SHA-1/SHA-256
17. `labs/ch19_public_key` — RSA、DH
18. `labs/ch20_pkcs` — 填充与 KDF
19. `labs/ch21_implementation` — 填充 oracle
20. `labs/ch22_examples` — 口令→KDF→加密+MAC 信封

概念章：第 1、10、23 章（阅读，无 lab）。

## 每章怎么学

```powershell
cd labs/chXX_...
mingw32-make test    # 基线
# 读 README + 对照源码改
mingw32-make test    # 回归
```

顶层一键：`mingw32-make test`（在仓库根目录）。

## 阅读时的现代对照（必记）

| 书中 | 今天 |
|------|------|
| DES | 废弃 → AES-GCM |
| MD5 / SHA-1 | 不可签名 → SHA-256+ |
| RC4 | 禁用 |
| 56-bit 密钥 | ≥128-bit |
| 教科书 RSA | OAEP/PSS |
