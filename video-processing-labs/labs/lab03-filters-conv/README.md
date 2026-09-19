# Lab 03 — 滤波器与卷积

对应路线图第 3 章。直接卷积、FFT 快速卷积、FIR/IIR（RBJ biquad）。

## 知识点

- 线性卷积 vs 频域循环卷积（补零到 L≥N+M-1）
- FIR 窗函数法设计与线性相位
- RBJ cookbook biquad（lowpass/peaking/shelf）
- 零极点与幅频响应

## 构建与运行

```bash
make
make test
make run
make clean
```

## 过关判据

| 指标 | 判据 |
|------|------|
| 快速卷积 vs 直接卷积 | 相对误差 < 1e-9 |
| 大 N 耗时对比 | N=65536/M=2049 时 FFT 卷积提速 ≥2×（实测 ~5.9×；FFT 为本 lab 演化的迭代 radix-2） |
| biquad 低通 -3 dB | 与设计截止频率偏差 < 2%（500/1k/2k/4k 扫描） |
| IIR 零极点 | 4 个截止频率下极点半径均 < 1（稳定），极点角频率位于 w0 下方（RBJ Q 约定，README 注记） |
| FIR 线性相位 | 冲激响应严格对称（含 64 抽头偶数情形）+ 通带群延迟波动 ≤ 0.5 样点 |

> 偶数抽头窗函数法 FIR 的对称中心是 (N−1)/2（分数），若按整型 N/2 取中心会破坏线性相位 —— 设计器已按分数中心实现，64 抽头情形纳入自检。

## 产物

- `out/fir_lpf_response.pgm` — FIR 幅频响应曲线
- `out/eq_demo.wav` — 三段 EQ 处理后的音乐/多音试听件
