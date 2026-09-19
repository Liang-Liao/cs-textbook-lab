# Lab 16 — 感知音频编码（MDCT + 心理声学）

MDCT/IMDCT TDAC 完美重构；简化心理声学（Bark / 扩展函数 / SMR）；
按 SMR 分配量化步长的玩具感知编码器；与 ADPCM / 均匀量化 MDCT 的噪声-掩蔽对照。

## 知识点

1. **MDCT + 50% 重叠 TDAC**：正弦窗 + OLA 实时域混叠消除，相对能量误差 < 1e-9。
2. **Bark 频带**：`z(f)=13·atan(0.00076f)+3.5·atan((f/7500)²)`，把频谱分组到约 1 Bark 临界带。
3. **扩展函数（spreading）**：在 Bark 域不对称卷积——向高频约 −10 dB/Bark，向低频约 −27 dB/Bark。
4. **掩蔽阈值与 SMR**：扩展后的带能量 × SNR offset（约 12 dB）得到阈值；SMR = 信号带能量 / 阈值。
5. **噪声整形比特分配**：均匀量化噪声功率 ≈ Δ²/12；按带内 `Δ ≤ √(12·T/n_bins)` 反推比特数，使量化噪声贴着掩蔽阈值走。
6. **感知 vs 波形**：目标不是全局 SNR，而是「噪声落在阈值之下」；ADPCM 可能 SNR 不低，但噪声谱会露出掩蔽阈。
7. **码流玩具**：每帧边信息（带比特数 + scale）+ 量化系数 Huffman（复用 lab14），闭环解码。

## 运行

```bash
make && make test    # 自检
make run             # 自检 + 写 out/noise_vs_threshold.pgm
```

工具链：`C:/msys64/ucrt64`（MSYS2 默认安装路径）下 `gcc` + `mingw32-make`（或 PATH 中的同名命令）。

## 产物

| 文件 | 说明 |
|------|------|
| `out/lab16` | 可执行文件 |
| `out/noise_vs_threshold.pgm` | STFT 谱（底图）+ 掩蔽阈（白线）+ 量化噪声（浅灰）叠加；`--generate` 写出 |

## 过关判据

| 指标 | 判据 | 实测（make test） |
|------|------|-------------------|
| MDCT 往返 | 相对能量误差 < 1e-9 | 自检 PASS |
| 心理声学 | 阈值有限且 >0；Bark 带数 ≥ 16；存在 SMR>1 | 自检 PASS |
| 感知编码 | MDCT 带内量化噪声 < 掩蔽阈 的比例 ≥ 70% | 自检 PASS |
| 重构可用性 | 时域 SNR ≥ 8 dB | 自检 PASS |
| 对照 | 感知噪声入阈比例高于 ADPCM（同为 4 bit 量级码率） | 自检 PASS |
| PGM | `out/noise_vs_threshold.pgm` | `--generate` |
