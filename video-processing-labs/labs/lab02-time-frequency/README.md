# Lab 02 — 时频分析（DFT / FFT / STFT）

对应路线图第 2 章。手写 DFT/FFT，并用 Hann STFT + OLA 做完美重构。

## 知识点

- DFT：\(X[k]=\sum_{n=0}^{N-1}x[n]e^{-j2\pi kn/N}\)，分辨率 \(\Delta f=f_s/N\)
- FFT：偶奇分治 \(O(N\log N)\)，与 DFT 逐点对账
- 窗与 COLA：Hann 50% 重叠满足 \(\sum_m w[n-mH]=\mathrm{const}\)
- STFT 分析/合成骨架（后续 NS/AEC/感知编码的基础）

## 构建与运行

```bash
make          # 构建 out/lab02
make test     # 过关判据自检
make run      # 生成频谱图 PGM
make clean
```

## 过关判据

| 指标 | 判据 |
|------|------|
| FFT vs DFT | 相对幅度误差 < 1e-9 |
| STFT 往返 | 相对能量误差 < 1e-10（Hann 50% OLA） |
| 复杂度 | N=1024 时 FFT 耗时 < DFT 的 1/50 |

## 产物

- `out/sweep_spectrogram.pgm` — 扫频时频轨迹
- `out/chirp_bird.pgm` — 多段轨迹热图
