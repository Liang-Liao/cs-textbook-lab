# 音视频开发实验室

基于 `docs/音视频开发理论知识路线图.md` 的 C 语言动手实验课。每个 lab 对应一章，自包含、可单独 `make test`。

## 环境

- Windows + MSYS2 UCRT64（gcc / make）
- PowerShell：先 `. .\scripts\env.ps1`
- MSYS2 bash：`source scripts/env.sh`

## 使用

```bash
cd labs/lab01-sampling-quantization
make          # 构建
make test     # 过关判据自检
make run      # 生成 out/ 下的 WAV/PGM
make clean
```

## Lab 索引

所有 lab 位于 `labs/` 目录下。

| Lab | 目录 | 章节 |
|-----|------|------|
| 01 | `labs/lab01-sampling-quantization` | 采样与量化 |
| 02 | `labs/lab02-time-frequency` | 时频分析 |
| 03 | `labs/lab03-filters-conv` | 滤波器与卷积 |
| 04 | `labs/lab04-random-wiener` | 随机信号与维纳 |
| 05 | `labs/lab05-audio-features` | 听觉感知与特征 |
| 06 | `labs/lab06-lpc-vad` | LPC 与 VAD |
| 07 | `labs/lab07-noise-suppress` | 噪声抑制 |
| 08 | `labs/lab08-aec` | 声学回声消除 |
| 09 | `labs/lab09-agc` | 自动增益控制 |
| 10 | `labs/lab10-mic-array` | 声源定位（选做） |
| 11 | `labs/lab11-color-yuv` | 颜色与 YUV |
| 12 | `labs/lab12-image-ops` | 图像处理与质量评价 |
| 13 | `labs/lab13-motion-estimation` | 运动估计 |
| 14 | `labs/lab14-transform-coding` | 变换量化熵编码 |
| 15 | `labs/lab15-toy-codec` | 玩具视频编码器 + ADPCM |
| 16 | `labs/lab16-perceptual-audio` | 感知音频编码 |
| 17 | `labs/lab17-mp4-container` | MP4 容器 |
| 18 | `labs/lab18-rtp-transport` | RTP 传输 |
| 19 | `labs/lab19-e2e-integration` | 端到端集成 |

设计说明见 `docs/compose/spec/av-labs-curriculum.md`。
