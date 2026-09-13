# Lab 14 — 第14章 Blowfish（结构教学版）

对应书中**第 14 章**：Blowfish 的 16 轮 Feistel、P/S 盒、密钥编排。

## 重要说明

为控制体积，本 lab 的 **S 盒初值为约简构造**（非 Schneier 公布的完整 π 表），
因此**不会**匹配官方 Blowfish 测试向量。目标是：

- 掌握 Feistel + F 函数（S0..S3 查表相加异或）
- 掌握 521 次加密式密钥编排流程
- 会做 CBC 多块回环与雪崩观察

若需官方 KAT，请对照完整 π 常量替换 `blowfish_init` 中约简的 S 盒构造。

## 文件

| 文件 | 内容 |
|------|------|
| `blowfish.c/.h` | 教育版 Blowfish |
| `test_ch14.c` | 回环、雪崩、CBC |

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 动手实验

1. 打印 16 轮后 `Xl/Xr`，对照书中轮结构
2. 改密钥长度 4→8，观察密文变化
3. 用本 lab 做 ECB 图像分块（可接 ch09 模式代码思路）

## 思考题

- Blowfish 密钥编排为何昂贵？对弱密钥有何影响？
- 与 DES S 盒设计目标的异同？
- 今天为何更多用 AES 而不是 Blowfish？

## 现代对照

Blowfish 已少用；Schneier 后续有 Twofish，主流为 AES。
