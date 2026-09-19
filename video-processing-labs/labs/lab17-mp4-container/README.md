# Lab 17 — MP4 容器解剖

自造最小合法 MP4（ftyp + mdat + moov），递归 box 树解析，样本表随机访问，PTS/DTS 堆重排。

## 知识点

1. ISO-BMFF 盒子：`size(4) + type(4) + payload`；容器盒（moov/trak/mdia/minf/stbl）可嵌套。
2. 样本表：`stsz` 帧大小、`stco` chunk 偏移、`stts` 时间戳增量；随机访问 = chunk_off + 前缀和。
3. 递归遍历比顶层 4 字节扫描可靠：`stsz` 藏在 `moov/trak/mdia/minf/stbl` 深层。
4. **stss 同步样本表**：1-based 关键帧帧号；随机访问/seek 从最近关键帧开始解码。
5. PTS（显示）与 DTS（解码）分离时用最小堆按 DTS 重建解码序，再按 PTS 得显示序（B 帧重排）。

## 实验

| 变体 | 帧数 | 尺寸 | timescale |
|------|------|------|-----------|
| `out/v0_tiny.mp4` | 8 | 32+8i | 1000 |
| `out/v1_mid.mp4` | 16 | 48+4i | 30000 |
| `out/v2_long.mp4` | 24 | 16+6i | 90000 |

每个变体：递归解析全部 box（验证深度 ≥4 能打到 stbl 子盒）→ 样本表逐帧 `fseek+fread` 抽查载荷。

PTS/DTS：喂入乱序的 `{DTS,PTS,id}`，断言 DTS 堆输出解码序、PTS 堆输出显示序。

I/P/B 统计：`out/ipb.mp4`（I/P/B 帧大小 400/200/60 量级，关键帧每 4 帧一个）→ 解析 stss 断言 `{1,5,9,13,17,21}`，按类打印帧数/均值/最小/最大（I > P > B）。

## 过关判据

| 指标 | 判据 |
|------|------|
| ≥3 变体 box 树 | ftyp/moov/mdat/stsz/stco/stts 齐全且递归深度够 |
| 样本表随机访问 | 三变体逐帧偏移/大小/载荷一致 |
| stss 关键帧索引 | 解析出的同步样本号与 muxer 写入模式一致 |
| I/P/B 帧大小统计 | 类计数与均值排序（I > P > B）与构造一致 |
| PTS/DTS 重排 | 解码序与显示序均与构造真值一致 |

```bash
make && make test
```
