# Lab 19 — 端到端集成（M3）

自包含拷贝最小模块，组装「采集 → VAD → AEC → NS → AGC → ADPCM → 打包 → 信道 → JB/PLC → 解码 → 落盘」全链；
双线程有界/无界队列对照；逐级探针定位注入 bug；音频主时钟 A/V 同步。

## 链图

```
capture sim (far speech → echo path → mic; near speech mid-stream)
    │  probe: capture
    ▼
vad_lite ──► probe: vad          ← 门控 AGC 增益更新（静音保持）
    ▼
aec_lite ──► probe: aec          ← 远端参考 NLMS；mic 近端主导时冻结自适应
    ▼
ns_lite  ──► probe: ns
    ▼
agc      ──► probe: agc          ← bug 注入点 (injected_scale)
    ▼
adpcm_encode → pkt_pack → channel(loss 10%/delay 0-25ms/reorder 15%)
    │                            → jb_insert（媒体时间调度）
    ▼
jb_take(deadline = seq·10ms + 40ms) ──miss──► PLC（保持上一帧）
    │                probe: adpcm_encode / jb_decode
    ▼
output   ──► probe: output ──► out/chain_out.wav, out/chain_energy.ppm
```

## 自包含副本（无跨 lab 链接）

| 模块 | 来源 | 用途 |
|------|------|------|
| wav_io | lab09 | 写 PCM WAV |
| agc | lab09 简化 | 自动增益 + clip |
| ns_lite | 新（能量门控） | 轻量降噪 |
| adpcm | lab15 | IMA 风格 4bit |
| rtp_lite | lab18 裁剪 | 打包 + 简单 JB |
| aec_lite | lab08 裁剪 | 远端参考 NLMS + ERLE |
| vad_lite | lab06 思想裁剪 | 能量 VAD（快降慢升噪声底 + hangover） |
| channel_lite | lab18 中继思想 | 媒体时间信道：丢包/延迟/乱序（进程内堆） |
| avsync_lite | lab18 裁剪 | 音频主时钟 A/V 同步判决 |
| ppm_io | lab12 | 能量条 PPM + 解码 PPM 序列 |

## 双线程对照

- 有界 cap=8：满载丢帧，最大排队延迟有上界。
- 无界 hard_cap=10000：慢消费时延迟随 backlog 增长，不丢帧。

## 探针定位

每个 stage 累计 energy / peak / clipped。`probe_localize` 找第一个
「clip 超阈 或 相对前级 energy 跳变 > 8×」的模块名。
注入 `agc.injected_scale=12` → 定位到 `agc`；干净链不误报。

## 过关判据

| 指标 | 判据 |
|------|------|
| 全链 | 信道丢包 ~10% 全部被容忍（送达率 ≥ 85%），PLC 补帧触发，输出能量合理，WAV/PPM 落盘 |
| 集成 ERLE | 远端单独段 mic vs AEC 输出能量比 ≥ 15 dB（实测 ~20.7 dB） |
| 集成 VAD | 语音帧计数 > 0（VAD 真在链上工作） |
| 端到端延迟 | 媒体时间实测：均值 < 200ms 且峰值 < 100ms（实测 40ms = JB 目标） |
| 集成 AGC | 20dB 跳变后 1s 内到目标 ±2 dB（集成模块不退化） |
| 有界队列 | drop>0 且 max_delay 有上界；无界对照 drop==0 且 delay ≥ bounded |
| 探针 | 定位到 `agc`；干净链 no false positive |
| 视频线 | 96×72×16 全 I 编码→包→解码闭环 PSNR≥30 dB；**PPM 序列** `out/video_00..15.ppm` 回放 |
| 音画同步 | 音频主时钟判决：基线零丢帧、注入视频 100ms 延迟后丢陈旧帧、显示偏差 < 33ms |

## 局部放宽（S4 合同 4 记录）

- 视频线为全 I 帧（P 帧完整实现见 lab15）：M3 验收聚焦链路与判据，非编码器完整度。
- 信道为进程内媒体时间调度（堆），非真实 UDP：socket 层验收在 lab18 已闭环。
- NS 为能量门控 lite（非 lab07 谱域 NS）：集成验收重点是链路协同与判据，算法完整度在原 lab。
