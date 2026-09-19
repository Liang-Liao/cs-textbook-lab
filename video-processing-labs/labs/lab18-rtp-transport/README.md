# Lab 18 — RTP 传输与自适应 Jitter Buffer

真实 WinSock UDP 回环 + 可编排信道（延迟/丢包/乱序）+ 自适应 JB + GCC-lite（梯度+丢包双分支）+ PLC + 音频主时钟 A/V 同步。

## 知识点

1. RTP：V=2 固定 12 字节头（seq/ts/pt/marker/ssrc 逐字段打包解包自检）；本 lab 在 payload 前嵌 8 字节 send_ms 便于测延迟。
2. UDP 回环：`WSAStartup` → `socket(AF_INET,SOCK_DGRAM)` → `bind(127.0.0.1)` → `sendto/recvfrom`；`SO_REUSEADDR` + 端口扫描应对 bind 竞争。
3. 信道仿真：中继线程 recv → 按丢包率丢弃 → 延迟最小堆（due_ms）+ 乱序概率 → 到期转发。
4. 自适应 JB：RFC3550 式 jitter EMA → `target_ms = clamp(3*jitter+25, 20, 200)`；按期望 seq 弹出；欠载 = 到达晚于播放 deadline。
5. GCC-lite：delay-gradient 趋势线（EMA）+ reduce/increase 状态机 + **丢包分支**（丢包 >5% → 0.5 倍乘性降速，比梯度的 0.88 退避更激进）。剧本：梯度先抬头（先于丢包降速）→ 真实丢包相（丢包分支再压）→ 长恢复。GCC 还直接消费**实测信道**统计（UDP 到达的 OWD 梯度 + 实测丢包率）。
6. PLC 三档：静音 / 保持 / 线性外推，MSE 可排序。
7. **A/V 同步（音频主时钟）**：所有时钟折算到媒体毫秒（音频 ts/48、视频 ts/90）。音频主时钟读数 = 墙钟 − 播放延迟 L；视频帧在其内容时间到达主时钟读数时显示；到达时主时钟已越过内容时间 + 容差（1 视频帧 33ms）→ 判陈旧帧丢弃（重同步）。注入纯视频 100ms 延迟 → 模块按判决丢帧、显示帧偏差保持 < 1 帧。

## 数据通路

```
sender(tx) --UDP--> relay bind --[delay/loss/reorder heap]--> rx --UDP--> AJB playout
                                                            ↘ GCC（OWD 梯度 + 实测丢包）
```

## 过关判据

| 指标 | 判据 |
|------|------|
| RTP 字段 | seq/ts/pt/marker/ssrc 往返一致 + 头部位布局抽查 |
| UDP 回环 | 真实 sendto/recvfrom 字节一致 |
| 10% 丢 + 100ms 抖动 | 自适应 JB 欠载率 < 1%；e2e 延迟 < 200ms |
| GCC（剧本） | 梯度相先降速（onset < before）；丢失相乘性降速再压（after_loss < after_grad）；恢复尾 rate≥0.9 且 util≥80% |
| GCC（实测信道） | ~10% 实测丢包下 rate ≤ 0.9（不冲顶 1.2）且有降速事件 |
| PLC | extrap < hold < silence |
| A/V 同步 | 基线（无注入）零丢帧且显示偏差 < 33ms；注入视频 100ms 延迟后同步模块丢陈旧帧、显示偏差仍 < 33ms |

```bash
make && make test
```
