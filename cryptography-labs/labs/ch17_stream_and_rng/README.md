# Lab 17 — 第17章 其它流密码与真随机

对应书中**第 17 章**：随机性测试与熵源。

## 实验目标

- 会做最基础的随机性统计：**monobit（频率）**、**runs（游程）**、**字节直方图偏差**
- 对比系统 CSPRNG（BCrypt）与「全零」弱源：弱源立刻被测出
- 理解统计测试只能「发现明显偏差」，不能证明密码学安全

## 文件

| 文件 | 内容 |
|------|------|
| `rngtest.c/.h` | `rng_monobit_bias` / `rng_runs_ok`（FIPS 140-2 风格，需 ≥2500 字节样本）/ `rng_byte_max_dev` |
| `test_ch17.c` | 全零 vs CSPRNG 对照 + 确定性弱源（交替位、长游程） |

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 动手实验

1. 按**比特**交替填 `bit_i = i & 1`（不是 `buf[i]=i&1`），看 monobit 仍平衡但 runs 检验失败（游程全是长度 1）
2. 缩小 CSPRNG 样本到 16 字节，观察 monobit/直方图是否变得不稳定（`rng_runs_ok` 需要 ≥2500 字节，短样本会直接判不过）
3. 用计数器 `0,1,2,...` 当“随机”：注意样本数要**不是 256 的整倍数**（如 4097 字节），否则每个字节恰好出现同样次数、直方图偏差恰为 0，三项测试全过

## 思考题

- 为什么通过 NIST STS 也不能证明是 CSPRNG？
- A5/1 等流密钥的弱点是「统计偏差」还是「状态可恢复」？
- 系统熵池耗尽时，应用层应怎么做？

## 现代对照

现实用 `/dev/urandom`、`getrandom`、BCrypt/CryptGenRandom；测试参考 NIST SP 800-22 / TestU01。
