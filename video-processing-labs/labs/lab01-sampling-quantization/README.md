# Lab 01 — 采样与量化

对应路线图第 1 章。手写 WAV 读写器，观察混叠与量化 SNR。

## 知识点

- 采样定理与混叠折叠：\(f_{alias}=|f-k f_s|\) 折叠进 \([0,f_s/2]\)
- 量化信噪比：\(\mathrm{SNR}\approx 6.02B+1.76\) dB（满幅正弦）
- WAV/RIFF：`RIFF/WAVE` + `fmt ` + `data` 逐字节字段
- dither 可改善低 bit 量化听感/谱底（本 lab 做客观对比）

## 构建与运行

```bash
make          # 构建 out/lab01
make test     # 过关判据自检
make run      # 生成 out/ 下 WAV/PGM
make clean
```

## 过关判据

| 指标 | 判据 |
|------|------|
| 量化 SNR | 实测与 \(6.02B+1.76+20\log_{10}(A_{peak})\) 偏差 < 0.5 dB（16/12/8/4 bit） |
| 混叠折返 | **多音**信号（3 个 >fs/2 分量）每个分量的折叠谱线均高于局部底 ≥20 dB |
| dither 谱底对比 | 4 bit：dither 后量化误差与信号相关性 < 无 dither 的 70%，最大谐波残余显著下降（实测 22.2→12.9 dB） |
| WAV 往返 | 写读 header/样本一致 |

> signed mid-tread 量化器最高正电平为 \(A_{peak}=1-2^{1-B}\)。经典公式 \(6.02B+1.76\) 在 4 bit 时必须做幅度修正才能对齐；16/12/8 bit 修正量可忽略。自检同时打印 classic 与 peak-corrected 理论值。dither 的客观机理：TPDF 抖动使量化误差与信号去相关（谐波残余消失、谱底略升），自检以"误差-信号相关系数 + 谐波尖峰高度"量化。

## 产物

- `out/sine440.wav` — 参考正弦
- `out/alias_*.wav` — 不同 fs 混叠试听件
- `out/spectrum_*.pgm` — 频谱热图
- `out/quant_*.wav` — 各 bit 深量化结果
