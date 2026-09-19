---
feature: av-labs-curriculum
status: delivered
updated: 2026-09-13
branch: master
commits: 69e45b1..HEAD
---

# 音视频开发实验室（Lab 01–19）设计与规划

## Report

**What was built** — 19 个自包含 C lab（`labs/labNN-*`）：信号地基、3A、图像视频编码、容器/UDP 传输、端到端流水线。头文件与 `.c` 同放 `src/`。审查后的 G1–G14 缺口补齐已完成：去 oracle 噪声估计、真 TSS/GCC-PHAT、感知编码器、UDP 回环、探针定位等进入 `make test` 硬门。

**Verification** — 2026-01-17 Wave1–3：`mingw32-make` 全量 19 lab PASS。Wave1：CIF / GCC util / 视频线。Wave2：Huffman / A/V sync。Wave3：provenance 标注 + Lab08 PBFDAF（L=256 ERLE≥15 dB）。

**2026-09-13 路线图符合性复审（第二轮）** — 静态审计 + 逐项修复后全量 `mingw32-make test` 19/19 PASS。本轮把"对照 spec 验收通过、对照路线图原文仍缺失"的判据补齐：

- **Lab08**：双讲检测改为信号驱动（残差/回声估计能量比 + min-statistics 阈值校准 + 承诺探测），NLP 抑制方向修正（残差占比越高抑制越深，floor 可达），新增 NLP 后 ERLE 硬门（≥20 dB 且比线性 AEC 多 ≥3 dB），PBFDAF 升到 L=512/P=4（对齐路线图 512+ 抽头；跨分区总功率归一化修复发散）并以迭代 radix-2 FFT 实测提速 ~4×，μ 扫描落盘 CSV。
- **Lab09**：压缩器扫幅对账入测（-60..0 dBFS 逐点 vs 解析曲线）；新增前瞻软限幅器，AGC+限幅器串联在真实 0 dBFS 边界削波数==0；超调按攻/释放两组量化。
- **Lab18**：音画同步重做（音频主时钟 + 媒体时间映射 + 迟到判决，注入视频 100ms 延迟后丢陈旧帧且显示偏差 <1 帧）；GCC 增加丢包分支（>5% → 0.5 倍乘性降速）并消费实测信道 OWD 梯度与丢包率；RTP 补 SSRC/marker 字段自检。
- **Lab19（M3）**：VAD+AEC 入链（链序 VAD→AEC→NS→AGC→ADPCM→信道→JB/PLC→解码），新增集成 ERLE≥15 dB（实测 20.7）、AGC 收敛保持、真实进程内信道（10% 丢包+延迟+乱序）+ PLC 补帧、端到端媒体延迟实测（40ms）、视频线 PPM 序列回放。
- **Lab07**：WebRTC 流水线真在线化（LR 似然比 + 能量联合判决门控噪声跟踪；发现并修复 minstat 环形缓冲零种子导致的 N2 坍缩），谱减 α/β 3×3 扫描 + 谱平坦度音乐噪声趋势硬门。
- **Lab15**：帧内三模式改为 RDO 式试算择优（量化重建 SSD 最小），新增 DC-only 对照（首帧 +0.27 dB 硬门）；闭环接 SSIM 复评。
- **Lab10**：新增信号级 4 麦 DAS（分数时延传播→导向对齐→求和），设计频率处目标/干扰增益差 11.6 dB 硬门。
- **Lab04**：高斯统计改为 10 段合并 z 分数（0.05·SE 字面界先验通过率仅 4%，属种子抽奖）；新增估计-Ps 维纳对照（oracle 13.7 vs 估计 5.7 dB，量化"无先验做不到"）；删除 out/findseed.*。
- **Lab01/03/05/11/12/14/17**：多音混叠逐分量验证、dither 谱底对比；卷积耗时对比（迭代 FFT 5.9×）+ IIR 极点扫描 + 偶数抽头 FIR 对称中心修复；掩蔽幅度扫描（临界频带排序硬门）；高饱和边缘区色度 PSNR（9.5 vs 平坦 172.9 dB）；PPM/PGM reader 正式化 + 头校验；JPEG 量化矩阵 + 高 QP 块效应图 + Huffman 频次 uint32 修复；stss 关键帧索引 + I/P/B 帧大小分布。

**Journey log**
- 量化 SNR 需按 mid-tread 峰值 \(1-2^{1-B}\) 修正，否则 4 bit 对不齐 \(6.02B+1.76\)。
- STFT 完美重构用周期 Hann + hop=N/2 + double 累加。
- 运动估计 TSS/菱形在强噪声纹理上易陷局部极小；全搜索保证精度，快速搜索单独报告恢复率。
- MP4 样本表需按 box 标签扫描，不能只扫顶层。
- 子代理无法代跑 make 时，由父会话统一编译验收。
- `make test` 全绿 ≠ 路线图达标：oracle PSD、别名算法、静默放宽阈值都会虚高完成度。
- NS：噪声估计绝不能被语音帧抬高；先验噪声前缀 + 禁止 N2 上抬是关键。
- 玩具 I 帧编解码必须用**重建邻域**做帧内预测，否则闭环 PSNR 会掉到个位数。
- 有界队列的结束哨兵不能走满队丢帧路径，否则消费者死锁。
- 双讲"高残差"有歧义（近端语音 vs 滤波器未收敛）：硬冻结会死锁，泄漏步长恢复太慢；能量比 + min-stat 阈值 + 承诺探测自洽。逐分区归一化的 PBFDAF 在 P=4 发散，必须用跨分区总功率归一化。
- 递归 FFT 的 per-level malloc 在"11 FFT/块"的 PBFDAF 场景占满时间预算——迭代 radix-2 提速 ~9×。
- 逐样本 e²/ŷ² 在远端包络低谷处发散，双讲统计量必须用块级池化能量比。
- SAD 模式选择与量化后真实失真可能相反；模式择优要用 RDO（量化重建 SSD）。
- 4:2:0 色度损失测量：彩条必须与子采样网格错相（shift 3px），否则测出的是假无损。
- 统计判据写成 |μ|<0.05·SE 时先验通过率 4%，只能改为 CI 检验——判据设计要考虑统计功效。

## [S1] Problem

以 `docs/音视频开发理论知识路线图.md` 为唯一教学主线，把 19 章理论路线落成可编译、可自测、可听/可看的 C 语言实验。目标不是调用现成编解码库，而是亲手实现关键算法，建立「采样 → 分析 → 处理 → 编码 → 容器 → 传输 → 集成」的完整体感。

约束（已与用户确认）：

- 路径：全量 1→19（第 10 章标选做，不阻塞后续）。
- 语言与依赖：C11；不引入第三方库；系统库（`libm`、stdio、以及第 18/19 章的 socket/pthread 等 OS API）允许。
- 环境：Windows + MSYS2 UCRT64（gcc 16.1.0、make、cmake，MSYS2 默认安装于 `C:\msys64`）。
- Lab 独立：每个 lab 自包含，单独 `make` / `make test` 可通过；lab 之间无编译链接依赖。
- 节奏：设计文档定稿后，从 Lab 01 起逐章实现并验收过关判据，再进下一章。

## [S2] Design

### 2.1 总体原则

1. **一章一 Lab**：目录 `labNN-<slug>/`，对应路线图第 N 章；实现范围严格对齐该章「编码实验 + 过关判据」。
2. **自包含**：后续章节若需要前序算法（WAV、FFT、卷积等），将**最小可运行副本**拷入本 lab 的 `src/`（文件头注明 `copied from labNN`），禁止跨目录 `#include` 或链接兄弟 lab。
3. **零第三方依赖**：可视化用 PPM/PGM；音频用 WAV（PCM s16le）；网络用本地 UDP 回环。测试数据**程序合成**，不依赖外部媒体文件（第 17 章可额外提供可选样例路径，默认仍自造最小合法 MP4）。
4. **自检即验收**：每个 lab 必须有 `make test`，退出码 0 表示全部过关判据满足；stdout 打印「指标名 / 实测值 / 判据 / PASS|FAIL」。
5. **学习产物可见**：关键实验默认把 WAV / PGM / PPM / 数值曲线写到 `out/`，便于听/看/对账。

### 2.2 工具链与调用方式

| 项 | 值 |
|----|----|
| 编译器 | `C:\msys64\ucrt64\bin\gcc.exe`（C11，按实际安装路径调整） |
| make | `C:\msys64\usr\bin\make.exe`（同上） |
| 数学库 | `-lm`（ucrt64 下通常随 gcc 默认链入，Makefile 显式写上） |
| 推荐入口 | 在 MSYS2 UCRT64 shell 中：`cd labNN-... && make test` |
| PowerShell 入口 | 先 `scripts/env.ps1` 注入 PATH，再 `make -C labNN-... test` |

`scripts/env.ps1` 与 `scripts/env.sh` 提供可复用的 PATH 片段，避免手写全路径。

公共编译选项（各 lab Makefile 继承同一组，可按需追加）：

```make
CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Werror -Isrc
LDLIBS  ?= -lm
```

第 18 章追加 `-lws2_32`（WinSock）；第 19 章追加 `-pthread`（ucrt64 winpthreads）。

### 2.3 仓库布局

```text
video-processing-labs/
├── docs/
│   ├── 音视频开发理论知识路线图.md    # 已有，唯一理论主线
│   └── compose/spec/av-labs-curriculum.md
├── scripts/
│   ├── env.ps1
│   └── env.sh
├── labs/
│   ├── lab01-sampling-quantization/
│   ├── lab02-time-frequency/
│   ├── ...
│   └── lab19-e2e-integration/
└── README.md                        # 路径说明 + lab 索引
```

每个 lab 的内部骨架：

```text
labNN-name/
├── README.md      # 本章目标、知识点提要、实验步骤、过关判据、如何跑
├── Makefile       # all / run / test / clean；out/ 自动创建
├── src/           # main + 模块 + 头文件 + 自包含副本（.h 与 .c 同目录）
├── tools/         # 可选：生成数据、批量对比的小命令
└── out/           # gitignore 产物；make clean 可删
```

> 注：原先每 lab 的 `include/` 已合并进 `src/`；Makefile 使用 `-Isrc`。

### 2.4 自包含复用策略（copied utilities）

允许拷贝的「基础设施模块」清单（每份带 provenance 注释，并按本 lab 需要裁剪）：

| 模块 | 首创 lab | 被谁拷贝 |
|------|----------|----------|
| `wav_io`（RIFF 读写 s16le/stereo） | 01 | 03,04,05,06,07,08,09,15,16,19 |
| `pgm_io` / `ppm_io` | 01/11 | 02–05,11–15,19 等 |
| `fft`（复数 FFT） | 02 | 03,05,07,08,10,16,18,19 |
| `stft`（分析/合成 + Hann） | 02 | 05,07,08,16,19 |
| `biquad` / `fir` | 03 | 09,19 |
| `rng_gauss` / `snr_tools` | 04 | 06,07,08,19 |
| `huffman` | 14 | 15,16,19 |
| `yuv_io` | 11 | 12,13,15,19 |

规则：

- 拷贝时**只保留本 lab 用到的 API**，避免把上一章整包拖进来。
- 若算法在本章有改进（如迭代 FFT 替换递归 FFT），在本 lab 内演进，不回写旧 lab。
- 不在 labs 间建立 `common/` 静态库（已确认不用共享库方案）。

### 2.5 自检（selftest）约定

- 入口：`make test` → 运行 `out/<lab>_test` 或 `out/main --selftest`。
- 输出行格式：`[PASS] <metric>=<value> (criterion <op> <threshold>)` 或 `[FAIL] ...`。
- 汇总：`ALL TESTS PASSED` 或 `FAILED n/m`，进程退出码 0/1。
- 数值判据优先用路线图原文阈值；浮点比较使用明确容差（写在代码常量与打印中）。
- 允许「定性对比」类实验输出报告文件 + 关键数值门槛双验收（例如频谱图存在且 SNR 提升达标）。

### 2.6 数据与可视化约定

- 音频：`out/*.wav`，PCM 16-bit LE，采样率 8k/16k/44.1k/48k 按章节需要。
- 图像：`out/*.pgm`（灰度）/ `out/*.ppm`（RGB），二进制 P5/P6。
- 曲线/频谱：可用同一像素缓冲画折线/热图再写 PGM，避免依赖 gnuplot。
- 随机性：`srand` 固定种子（写进 selftest 打印），保证可复现。

### 2.7 Windows / MSYS2 细节

- 文件 I/O 一律 `"rb"` / `"wb"`，避免文本模式换行破坏二进制。
- 路径在源码中使用相对路径 + `/` 分隔符。
- 第 18 章：`WSAStartup` 初始化；socket 用 `AF_INET/SOCK_DGRAM` 本地回环。
- 第 19 章并发：`-pthread` + POSIX 线程 API（ucrt64 支持）；环形缓冲用互斥锁 + 条件变量。
- 不依赖 shell 外部命令做核心验证；`make test` 自闭环。

### 2.8 Lab 目录总表

| Lab | 目录名 | 路线图章 | 核心产出 | 主要自检门槛 |
|-----|--------|----------|----------|--------------|
| 01 | `lab01-sampling-quantization` | 1 采样量化 | WAV 读写器、混叠与量化实验 | SNR 与 6.02B+1.76 偏差&lt;0.5 dB；混叠折返位置正确 |
| 02 | `lab02-time-frequency` | 2 时频分析 | DFT/FFT/STFT 库、频谱图 | FFT vs DFT 误差&lt;1e-9；STFT 往返&lt;1e-10；FFT/DFT 耗时比≥50 |
| 03 | `lab03-filters-conv` | 3 滤波器与卷积 | FIR/IIR/biquad、频域卷积 | 卷积误差&lt;1e-9；biquad -3dB 偏差&lt;2% |
| 04 | `lab04-random-wiener` | 4 随机信号 | 高斯噪声、AR(1)、频域维纳 | 统计检验通过；已知 Ps/Pn 时 SNR 提升≥3 dB |
| 05 | `lab05-audio-features` | 5 感知与特征 | Mel 滤波组、简化 LUFS | Mel 中心频率单调符合 mel 尺度；增益翻倍 → +6 LU |
| 06 | `lab06-lpc-vad` | 6 LPC 与 VAD | 双门限 VAD、GMM-EM、LPC | SNR≥10 dB 时端点误差≤80 ms；合成元音共振峰误差≤5% |
| 07 | `lab07-noise-suppress` | 7 NS | 谱减、DD-Wiener、最小值跟踪、简化 WebRTC NS | 输入 5 dB → 输出≥10 dB；噪声突升跟踪≤1 s |
| 08 | `lab08-aec` | 8 AEC | NLMS/PBFDAF、延迟估计、双讲冻结、NLP | 单讲 ERLE≥20 dB；延迟误差 0 样点 |
| 09 | `lab09-agc` | 9 AGC | 压缩器、AGC、限幅 | 跳变后≤1 s 到目标±2 dB；无削波；轻/响段 LUFS 差≤1 |
| 10 | `lab10-mic-array`（选做） | 10 阵列 | GCC-PHAT、延迟求和 | ±60° 误差≤5°；目标/抑制增益差≥6 dB |
| 11 | `lab11-color-yuv` | 11 颜色与 YUV | RGB↔YUV420、色条错配演示 | 往返误差≤2 LSB |
| 12 | `lab12-image-ops` | 12 图像处理 | 2D 卷积、边缘、缩放、SSIM | 可分离卷积误差&lt;1e-9；SSIM 对增益不敏感性可证 |
| 13 | `lab13-motion-estimation` | 13 运动估计 | 全搜索/三步法、半像素 MC | 纯平移恢复≤1 px；三步法提速≥10× 同等恢复 |
| 14 | `lab14-transform-coding` | 14 变换量化熵编码 | 8×8 DCT、量化、Huffman、Zigzag+RLC | IDCT 闭环&lt;1e-6；Huffman 无损；压缩比&gt;2:1 |
| 15 | `lab15-toy-codec` | 15 编码框架 | 玩具视频编码器（I/P）+ ADPCM | CIF 中等 QP PSNR≥30 dB；ADPCM SNR≥25 dB |
| 16 | `lab16-perceptual-audio` | 16 感知音频编码 | MDCT、心理声学、比特分配 | MDCT 完美重构&lt;1e-9；噪声谱主体在掩蔽阈下 |
| 17 | `lab17-mp4-container` | 17 MP4 容器 | box 树解析、样本表、PTS/DTS | ≥3 个自造/样例 MP4 解析一致；随机访问帧定位成功 |
| 18 | `lab18-rtp-transport` | 18 传输 | RTP 收发、jitter buffer、PLC、GCC 简化 | 10% 丢包+100ms 抖动欠载&lt;1%；GCC 先于丢包降速 |
| 19 | `lab19-e2e-integration` | 19 端到端 | 双线程流水线 + 全链组装 | 延迟有界；ERLE/AGC 不退化；探针定位注入 bug |

里程碑（组合验收，不新建独立 lab，而是在对应 lab 的 README 中标注组合方式，并在下列 lab 的 `make test` 中加「组合烟雾项」）：

- **M1**：lab06→08→07→09 脚本化串联（音频 3A 链），在 lab09 或单独 `milestones/m1-3a/` 做轻量组合入口（仍拷贝源码，不链接）。
- **M2**：lab13→14→15→16，视频玩具编码器 RD + 音频感知编码对照。
- **M3**：lab19 正式集成；里程碑入口即 lab19。

> 里程碑目录若创建，仅作「多 lab 源码拷贝的组装实验场」，不反向成为其他 lab 的依赖。

### 2.9 每章 Lab 内容规格（实现范围）

#### Lab 01 — 采样与量化 `lab01-sampling-quantization`

- 实现 `wav_io`：逐字节解析/写出 RIFF、`fmt `、`data`；校验 header。
- 合成多音正弦 → 不同 fs 采样 → 混叠 WAV + 频谱热图 PGM。
- 同一信号 16/12/8/4 bit 量化，测 SNR；可选 dither 对比。
- **自检**：SNR 公式偏差；混叠峰频位置 vs 理论折叠频率。

#### Lab 02 — DFT/FFT/STFT `lab02-time-frequency`

- O(N²) DFT、递归 FFT（选做迭代蝶形）、Hann STFT + OLA iSTFT。
- 频谱图 dB 热图（扫频/合成鸟鸣轨迹）。
- **自检**：DFT/FFT 对账、STFT 往返能量误差、复杂度耗时比。

#### Lab 03 — 滤波器与卷积 `lab03-filters-conv`

- 直接卷积 vs FFT 快速卷积；窗函数法 FIR 低通；RBJ biquad 三段 EQ；极点单位圆与幅频响应。
- **自检**：卷积一致、-3 dB 点、线性相位群延迟。

#### Lab 04 — 随机信号与维纳 `lab04-random-wiener`

- Box-Muller、直方图 PGM、按目标 SNR 加噪工具、频域维纳、AR(1) 有色噪声。
- **自检**：均值/方差门槛；已知 Ps/Pn 维纳 SNR 提升≥3 dB（0 dB 输入）。

#### Lab 05 — 听觉特征 `lab05-audio-features`

- Mel 三角滤波组 + Mel 谱热图；简化 LUFS（K 加权 + 门限）；掩蔽实验可用双纯音幅度扫描（客观阈值模型，不依赖真实听测）。
- **自检**：Mel 中心频率表；增益 +6 dB → LUFS +6 LU±0.5。

#### Lab 06 — LPC 与 VAD `lab06-lpc-vad`

- 短时能量 + 过零率双门限 VAD；合成语音/静音真值；GMM+EM 似然比 VAD；自相关 LPC + Levinson-Durbin；共振峰提取。
- **自检**：端点误差；GMM vs 门限在 0 dB 的优势量化；共振峰误差。

#### Lab 07 — 噪声抑制 `lab07-noise-suppress`

- 谱减、判决引导维纳、最小值跟踪噪声估计、简化 WebRTC 风格流水线；处理前后频谱 PGM。
- **自检**：5→≥10 dB SNR；噪声突升 1 s 内跟踪；语音段保护（增益不过度压语音）。

#### Lab 08 — AEC `lab08-aec`

- 合成回声数据集（已知 h_true）；NLMS；PBFDAF；频域互相关延迟估计；双讲检测 + 冻结；简易 NLP；ERLE 统计。
- **自检**：ERLE≥20 dB；延迟 0 误差；双讲近端损伤≤1 dB；μ 扫描曲线输出。

#### Lab 09 — AGC `lab09-agc`

- 静态压缩器（门限/比率/软拐点）；AGC 环路（攻击/释放）；限幅器；LUFS 量化轻/响段。
- **自检**：收敛时间与容差；peak &lt; 0 dBFS；LUFS 差≤1。

#### Lab 10 — 阵列（选做）`lab10-mic-array`

- 双麦仿真 + GCC-PHAT 测角；4 麦延迟求和波束图 PGM。
- **自检**：角度误差；目标/干扰增益差。
- **不阻塞** Lab 11+；缺省 `make test` 可标记 skip 若未启用，但默认完整实现（全量路径）。

#### Lab 11 — 颜色与 YUV `lab11-color-yuv`

- BT.601/709 可切换 RGB↔YUV420；色条错配演示；4:2:0 上下采样损失；直方图均衡。
- **自检**：往返≤2 LSB；色度边缘 PSNR 报告。

#### Lab 12 — 图像处理 `lab12-image-ops`

- PPM/PGM 正式化；2D 可分离卷积；Sobel；双线性缩放；SSIM 滑窗。
- **自检**：可分离误差；格点一致；SSIM vs PSNR 排序分歧构造例。

#### Lab 13 — 运动估计 `lab13-motion-estimation`

- 合成平移序列；SAD 全搜索；三步法；半像素 MC。
- **自检**：恢复误差；三步法时间/精度；半像素残差能量下降。

#### Lab 14 — 变换量化熵编码 `lab14-transform-coding`

- 8×8 DCT/IDCT；量化矩阵与 QP；Huffman 建树/编解码；Zigzag+RLC；整图压缩比。
- **自检**：变换往返；Huffman 无损；压缩比&gt;2:1。

#### Lab 15 — 玩具视频编码器 `lab15-toy-codec`

- I：帧内 DC/水平/垂直择优 + DCT 量化 + Huffman。
- P：块匹配 MC + 残差变换编码。
- 码率控制（简单 QP 调节）；RD 曲线；ADPCM（IMA 风格）。
- **自检**：PSNR≥30 dB；模式择优收益；ADPCM SNR≥25 dB；长序列无误差爆炸。

#### Lab 16 — 感知音频编码 `lab16-perceptual-audio`

- MDCT/IMDCT + TDAC 验证；简化心理声学（Bark/扩展函数/SMR）；按 SMR 分配步长；Huffman 拷贝自 lab14；与 ADPCM 客观/听感对照报告。
- **自检**：MDCT 完美重构；噪声 vs 掩蔽阈 PGM。

#### Lab 17 — MP4 容器 `lab17-mp4-container`

- 自造最小合法 MP4（ftyp+moov+mdat，自写 muxer）作为默认测试件，避免外网依赖；box 树解析器；stsz/stco/stts 索引；PTS/DTS 重排；（选做）TS 包解析。
- **自检**：多文件解析与十六进制抽查；随机读第 N 帧命中。

#### Lab 18 — RTP 传输 `lab18-rtp-transport`

- RTP 打包/解包 + UDP 本地回环；可编排信道（延迟/丢包/乱序）；自适应 jitter buffer；PLC 三档；GCC 简化（trendline + 状态机）；音画同步。
- **自检**：欠载率；ERLE 无关——看端到端延迟稳定；GCC 拥塞响应；同步偏差。

#### Lab 19 — 端到端集成 `lab19-e2e-integration`

- 采集模拟 → 有界环形队列 → 3A 链 → 编码 → RTP → jitter → 解码 → 落盘。
- 对照无界队列延迟爆炸；逐级探针；注入 bug 演练脚本。
- **自检**：指标保持；有界延迟；探针定位成功（自动化：注入可检测偏差并由 test 断言定位模块名）。

### 2.10 实现顺序与依赖（仅源码拷贝关系，非链接依赖）

```text
01 → 02 → 03 → 04 → 05 → 06 → 07 → 08 → 09
 ↘         ↘         ↘                ↗
  11 → 12 → 13 → 14 → 15 → 16 → 17 → 18 → 19
                 10（选做，可在 09 后任意插入）
```

严格串行实现顺序：**01,02,03,04,05,06,07,08,09,10,11,12,13,14,15,16,17,18,19**。

每个 lab 完成定义（DoD）：

1. `make` 无警告构建（`-Werror`）。
2. `make test` 全绿，打印实测值。
3. `README.md` 含：知识点 3–8 条、运行方法、产物列表、过关判据对照。
4. 不修改已完成 lab（除非发现基础 bug 需 hotfix，且 hotfix 不改变其公共行为契约）。

### 2.11 编码风格（轻量）

- C11；4 空格缩进；`snake_case`；头文件 include guard `LABNN_XXX_H`。
- 禁止未使用变量/参数触发 `-Werror`；公共函数声明于 `src/` 内同名 `.h`。
- 错误处理：返回 `int`（0 成功，非 0 失败）或明确 `bool`；文件操作检查 `fread/fwrite` 返回值。
- 不写大段注释；只在非显然的 WHY 处注释（例如 COLA 条件、延迟冻结原因）。

## [S3] Out of Scope

- 不调用 FFmpeg / libopus / libwebrtc / OpenCV / libsndfile 等第三方库。
- 不实现真实声卡/摄像头采集；第 19 章用文件节拍模拟。
- 不实现完整 H.264/H.265/AV1、完整 AAC/Opus、完整 WebRTC ICE/DTLS/SRTP。
- 不做 GUI；可视化全部落盘为 WAV/PGM/PPM/文本报告。
- 不做自动听测（PEAQ/MUSHRA）；听感验收用客观代理指标 + 用户自行播放 WAV。
- 不在本轮创建 GitHub CI；本地 `make test` 即验收。
- 第 10 章为选做，未完成时 lab19 不依赖它。

## [S4] Gap Remediation (2026-01-17 review)

Static review against the roadmap found that several labs passed `make test` only because thresholds were loosened or hard experiments were never wired into selftest. This amendment tightens those labs to the roadmap 编码实验 + 过关判据, without reopening already-solid labs (01–03, 05, 11).

### Contracts

1. **Selftest honesty**: every roadmap 过关判据 that the lab claims must appear as a hard `[PASS]/[FAIL]` check, not an informational print.
2. **No oracle cheating**: noise PSD / clean references used for Wiener or NS selftests must be estimated from the noisy signal (min-statistics / decision-directed / first-frame init), not from `clean − noisy`.
3. **Name the algorithm**: `me_tss`, `gcc_phat`, frequency-domain delay estimators, etc. must actually implement the named algorithm or be renamed if kept as aliases.
4. **Thresholds**: use roadmap numbers. Local relaxations are allowed only when recorded in the lab README with a one-line why.
5. **Lab19 integration**: copy minimal modules from earlier labs (self-contained rule stays); implement audio 3A→encode→packet→JB→decode and a probe that localizes an injected bug to a module name.

Out of scope for this pass: external media downloads, real sockets beyond WinSock UDP loopback, GUI, CI.

## Tasks

- [x] T0: 仓库骨架 — acceptance: `scripts/env.ps1`、`scripts/env.sh`、根 README 索引、`.gitignore`（忽略 `out/` 与构建产物）就位；在 UCRT64 环境下空跑 make 约定可用 (covers: S2.2, S2.3)
- [x] T1: Lab01 采样与量化 — acceptance: `labs/lab01-.../make test` 全绿，满足第 1 章过关判据 (covers: S2.9 Lab01)
- [x] T2: Lab02 时频分析 — acceptance: FFT/DFT/STFT 自检通过 (covers: S2.9 Lab02)
- [x] T3: Lab03 滤波器与卷积 — acceptance: 卷积/biquad/FIR 判据通过 (covers: S2.9 Lab03)
- [x] T4: Lab04 随机信号与维纳 — acceptance: 噪声统计与维纳 SNR 判据通过 (covers: S2.9 Lab04)
- [x] T5: Lab05 听觉特征 — acceptance: Mel/LUFS 判据通过 (covers: S2.9 Lab05)
- [x] T6: Lab06 LPC 与 VAD — acceptance: VAD/LPC 判据通过 (covers: S2.9 Lab06)
- [x] T7: Lab07 噪声抑制 — acceptance: 谱减/维纳/WebRTC 风格 NS 判据通过 (covers: S2.9 Lab07)
- [x] T8: Lab08 AEC — acceptance: NLMS/延迟/双讲/NLP/ERLE 判据通过 (covers: S2.9 Lab08)
- [x] T9: Lab09 AGC — acceptance: 收敛/削波/LUFS 判据通过 (covers: S2.9 Lab09)
- [x] T10: Lab10 阵列选做 — acceptance: GCC-PHAT 与波束形成判据通过 (covers: S2.9 Lab10)
- [x] T11: Lab11 颜色与 YUV — acceptance: RGB↔YUV 往返与错配演示就绪 (covers: S2.9 Lab11)
- [x] T12: Lab12 图像处理 — acceptance: 2D 卷积/边缘/缩放/SSIM 判据通过 (covers: S2.9 Lab12)
- [x] T13: Lab13 运动估计 — acceptance: 全搜索/三步法/半像素判据通过 (covers: S2.9 Lab13)
- [x] T14: Lab14 变换量化熵编码 — acceptance: DCT/量化/Huffman 判据通过 (covers: S2.9 Lab14)
- [x] T15: Lab15 玩具视频编码器与 ADPCM — acceptance: PSNR/ADPCM/RD 判据通过 (covers: S2.9 Lab15)
- [x] T16: Lab16 感知音频编码 — acceptance: MDCT/心理声学/对照判据通过 (covers: S2.9 Lab16)
- [x] T17: Lab17 MP4 容器 — acceptance: box 树与样本表解析自检通过 (covers: S2.9 Lab17)
- [x] T18: Lab18 RTP 传输 — acceptance: 回环/jitter/PLC/GCC 判据通过 (covers: S2.9 Lab18)
- [x] T19: Lab19 端到端集成 — acceptance: 流水线、全链、探针定位判据通过 (covers: S2.9 Lab19)

### Gap-fix tasks (S4)

- [x] G1: Lab04 均值阈值对齐路线图 + AR(1) 功率谱报告 — acceptance: `|μ|<0.05·σ/√N` 硬校验；AR(1) 输出 PGM/数值谱峰对比 (covers: S4; S2.9 Lab04)
- [x] G2: Lab06 GMM 0 dB 优劣硬校验 — acceptance: SNR 0 dB 时 GMM F1 ≥ 门限 VAD F1，且差值打印 (covers: S4; S2.9 Lab06)
- [x] G3: Lab07 去 oracle + 谱减入测 + 简化 WebRTC NS — acceptance: 噪声 PSD 从 noisy 估计；谱减 α/β 有硬指标；流水线输出 SNR≥10 dB（输入 5 dB）；语音段增益损伤≤1 dB (covers: S4; S2.9 Lab07)
- [x] G4: Lab08 延迟估计闭环 + NLP + μ 扫描 — acceptance: 估计延迟→对齐→ERLE≥20 dB；延迟误差 0；双讲近端损伤≤1 dB；输出 μ 扫描曲线数据 (covers: S4; S2.9 Lab08)
- [x] G5: Lab09 LUFS≤1 LU + 收敛窗口硬校验 — acceptance: 轻/响 LUFS 差≤1；跳变后 1 s 内到目标±2 dB 硬断言 (covers: S4; S2.9 Lab09)
- [x] G6: Lab10 自检改走 gcc_phat + 4 麦 DAS 图 — acceptance: DOA 用 gcc_phat；±60°误差≤5°；目标/干扰增益差≥6 dB (covers: S4; S2.9 Lab10)
- [x] G7: Lab12 可分离耗时 + 最近邻对比 — acceptance: 打印两趟 vs 直接耗时；最近邻/双线性产物与格点一致检查 (covers: S4; S2.9 Lab12)
- [x] G8: Lab13 真三步法 + 阈值对齐 + MV 场 — acceptance: TSS 非全搜索别名；大位移下 speedup≥10× 且恢复率≥0.9；写出 MV 场 PGM (covers: S4; S2.9 Lab13)
- [x] G9: Lab14 RLC + QP 扫描 — acceptance: zigzag+RLC+Huffman 无损往返；扫 QP 输出失真/码率数值 (covers: S4; S2.9 Lab14)
- [x] G10: Lab15 长序列 + 码控 + RD + 编解码一致 — acceptance: ≥32 帧闭环 PSNR≥30 dB；帧内预测用重建邻域保证编解码一致；简单码控；输出 RD 数值表 (covers: S4; S2.9 Lab15)
- [x] G11: Lab16 心理声学 + SMR 比特分配编码器 — acceptance: Bark/扩展函数/SMR 可复现；噪声谱主体在掩蔽阈下（PGM）；MDCT 往返保持 (covers: S4; S2.9 Lab16)
- [x] G12: Lab17 多变体 MP4 + PTS/DTS 重排 — acceptance: ≥3 个参数不同的自造 MP4 解析一致；PTS/DTS 堆重排正确 (covers: S4; S2.9 Lab17)
- [x] G13: Lab18 UDP 回环 + 延迟/乱序信道 + 自适应 JB — acceptance: 本地 UDP 收发；编排延迟/丢包/乱序；欠载率<1%；端到端延迟稳定；GCC 先于丢包降速剧本可复现 (covers: S4; S2.9 Lab18)
- [x] G14: Lab19 全链集成 + 探针定位 — acceptance: NS→AGC→ADPCM→打包→JB→解码链跑通；有界队列延迟有上界且丢帧；注入 bug 由探针定位到模块名 (covers: S4; S2.9 Lab19)

依赖说明：G 任务相互独立；G14 拷贝了 G3/G13 思路的精简模块。每个 G 的 acceptance 即「路线图硬判据 + make test 可复现」。

**局部放宽（已写入对应 README）**：
- Lab04：高斯均值界 0.05·σ/√N 是 0.05-SE 界（单次通过率 ~4%，种子抽奖）——硬门改为 10 段合并 z<2，字面量照常打印；findseed 工具已删除。
- Lab06：GMM"明显优于"按 spec G2 口径放宽为 F1 不劣于（gap≥0），差值打印。
- Lab07：minstat 跟踪门限 7→9 dB（40 帧窗刷新占 0.32 s，10 dB 字面值在 1 s 预算边缘）；已披露无 why → 现补记。
- Lab08：h_true 整形（h[0]=1、尾部压扁）保证互相关峰在块延迟处——"延迟误差 0"依赖此前提；PBFDAF 按 512 抽头实现（4×128），2026-09-13 起无放宽。
- Lab13：小位移 TSS 以块恢复率≥0.85 为硬门，max_err 仅作信息打印（纹理局部极小）；大位移用网格对齐位移 (8,-8) 验证 TSS≥90%。
- Lab15：快路径 176×144×64；CIF 352×288×16 子集已硬门（H2）。模式收益对照使用方向性合成内容（斜坡），平坦默认内容上收益趋零。
- Lab18：GCC 恢复硬门 rate≥0.9 且 util≥80%（H3，长恢复尾）。
- Lab19：音频链 + **全 I 视频线**（96×72×16，PSNR≥30；完整 I/P 在 lab15）；信道为进程内媒体时间调度（UDP 层在 lab18 验收）；NS 为能量门控 lite。

## [S5] Aftercare — remaining roadmap edges (planned)

S4 已把「硬实验没进 selftest」类问题收口。下面只处理**仍偏离路线图原文、且值得继续做**的边界。原则：保持 `make test` 可复现、自包含、零第三方库；放宽项若取消则改回路线图硬门。

### 边界盘点与可行性

| # | 边界 | 路线图要求 | 现状 | 可行性 | 建议 |
|---|------|------------|------|--------|------|
| A | Lab19 视频线 | 合成序列→玩具编码器→打包→传输→解码→PPM 回放 | 仅音频链 + 能量 PPM | **高**（模块可拷贝裁剪） | **P0 做** |
| B | Lab15 CIF 352×288 | CIF 中等 QP PSNR≥30 dB | 176×144×64 | **中**（约 4× 像素，selftest 需控时） | **P0 做**（fast 路径保留） |
| C | Lab18 GCC 稳态利用率 | 拥塞后恢复、利用率≥80% 不震荡 | 硬门 rate≥0.7，util≈59% | **中**（拉长恢复尾 + 调 increase） | **P0 做** |
| D | Lab15 残差 Huffman | 熵编码（lab14 复用） | zigzag+RLE 紧凑流（非 Huffman） | **中**（拷 lab14 最小 Huffman） | **P1 做** |
| E | Lab19 音画同步 | 音频主时钟 + 视频帧对齐 | 未接 | **中**（lab18 有同步器可裁剪） | **P1 做** |
| F | Lab08 PBFDAF | 长路径频域自适应 | L=64 NLMS，README 已说明 | **低优先** | 可选 P2 |
| G | 拷贝 provenance 注释 | `copied from labNN` | 多数副本无标注 | **低**（文档向） | P2 |
| H | g_fail 双计 | — | 部分 lab 显示层 | **低**（cosmetic） | P2 |

不建议本轮做：外网 MP4 样例、完整 WebRTC AEC3/ICE、真实声卡采集、CI。

### Aftercare tasks

- [x] H1: Lab19 视频线进 selftest — acceptance: 合成小序列经（裁剪版编码 + 包 + 解码）写出 PPM；闭环 PSNR≥30 dB；音频链与有界队列/探针指标不回归 (covers: S5-A; S2.9 Lab19)
- [x] H2: Lab15 CIF 路径 — acceptance: 快路径 176×144 PASS + CIF 352×288×16 子集 avg PSNR≥30 dB (covers: S5-B; S2.9 Lab15)
- [x] H3: Lab18 GCC 长恢复尾 — acceptance: rate≥0.9 且 util≥80% 硬门；其余指标不回归 (covers: S5-C; S2.9 Lab18)
- [x] H4: Lab15 残差 Huffman — acceptance: 量化系数经 Huffman 往返无损；与 RLE 对照打印 bit 数；闭环 PSNR 不退化 (covers: S5-D; S2.9 Lab15)
- [x] H5: Lab19 音画同步 — acceptance: 音频主时钟映射后 A/V 偏差 < 1 视频帧（33ms）硬门 (covers: S5-E; S2.9 Lab19)
- [x] H6: 文档与 provenance — acceptance: 被拷贝模块头注释 `copied from labNN`；spec 局部放宽段与 README 同步 (covers: S5-G)
- [x] H7: Lab08 PBFDAF — acceptance: overlap-save 分区频域自适应，L=256 路径 ERLE≥15 dB 硬门；README 标明已实现 (covers: S5-F)

依赖：H1 可拷贝 H2/H4 的 lab15 裁剪；H3 独立；H5 依赖 H1 有视频帧时基。

### 建议执行波次

1. **Wave 1（P0）**：H3（最快）→ H2 → H1（最大块）
2. **Wave 2（P1）**：H4 → H5
3. **Wave 3（P2，可选）**：H6 + PBFDAF

## 审核检查单（设计阶段）

请重点确认：

1. 范围是否接受全量 19 lab + 第 10 章选做但默认实现。
2. 目录名/命名风格是否接受（`labNN-slug`）。
3. 第 17 章默认用**自造最小 MP4**（不外网下载）是否可接受；若你手头有样例 MP4，可放到 `lab17-.../samples/` 并在 README 标注可选路径。
4. 第 18/19 章使用 WinSock / pthreads（系统 API）是否在「尽量不要第三方库」范围内。
5. 里程碑采用「组合烟雾项 + 文档说明」而非独立第 20 lab，是否接受。
