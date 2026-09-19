# Lab 15 — 玩具视频编码器 + ADPCM

I/P GOP、帧内预测 + DCT 量化 + Zigzag RLE 紧凑码流、整数 ME、简单码率控制、IMA 风格 ADPCM。

## 知识点

1. **GOP**：每 `gop` 帧一个 I 帧，其余为 P 帧；解码器以自身重建帧为参考（闭环）。
2. **帧内预测**：DC / 水平 / 垂直三模式，残差再做 8×8 DCT。
3. **运动估计**：块 SAD 全搜索（range=8），MV 用 signed exp-golomb。
4. **紧凑码流**：mode(2bit) 或 MV(se,se) + 残差 `DC se` 后 `(run:6, level se)*` + EOB；可量出 bpp 与压缩比。
5. **码率控制**：按目标 bits/frame 调整下一帧 QP（超出 +15% 升 QP，低于 85% 降 QP）。
6. **闭环 PSNR**：必须从码流解码并以重建帧为参考，而不是直接用编码端缓冲。
7. **ADPCM**：4-bit 自适应差分脉码调制，语音类合成信号 SNR ≥ 25 dB。

## 运行

```bash
make && make test
```

## 过关判据

| 指标 | 判据 |
|------|------|
| 序列规模 | 快路径 176×144×64；CIF 352×288×16 子集亦硬门 PSNR≥30 dB |
| 闭环解码 PSNR | 中等 QP(=10) 平均 ≥ 30 dB |
| RD 扫描 | QP ∈ {6,10,16}，打印 bpp/压缩比/PSNR |
| 码率控制 | 有目标码率时打印 QP 轨迹与达成码率 |
| 码流 | Huffman 残差（lab14）+ RLE 对照，均无损往返 |
| ADPCM | SNR ≥ 25 dB |

## 实现说明

- `src/dct.c` 孤儿 stub 已删除；DCT/IDCT 保留在 `codec.c`。
- 码流布局：`W:16 H:16 N:16 gop:8 base_qp:8`，随后每帧 `qp:6` + 块数据。
