# Lab 11 — 颜色与 YUV

对应路线图第 11 章。BT.601/BT.709 可切换 RGB↔YUV420，色条错配演示，4:2:0 色度损失与直方图均衡。

## 知识点

1. 有限幅（limited range）YCbCr：Y∈[16,235]，Cb/Cr∈[16,240]，避免满幅纯色溢出。
2. BT.601（SD）与 BT.709（HD）亮度权重不同；解码矩阵必须与编码矩阵配对。
3. 4:2:0 对色度 2×2 平均下采样、最近邻上采样；色度边缘（色条）PSNR 明显下降。
4. 把 YUV 缓冲直接当 RGB 显示会得到错色；用错标准矩阵会偏色。
5. 直方图均衡只作用于 Y，保留色度，避免色偏。

## 运行

```bash
make && make test    # 自检 + 写 out/ 产物
make run             # 仅生成色条演示
```

## 过关判据

| 指标 | 判据 |
|------|------|
| RGB→YUV(8bit)→RGB | max abs error ≤ 2 LSB（BT.601 与 BT.709） |
| 色条 4:2:0 往返 | 打印 chroma edge PSNR，且 > 18 dB |

## 产物

- `out/colorbars.ppm` — 原始 100% 色条
- `out/colorbars_420.ppm` — 4:2:0 往返
- `out/yuv_as_rgb_wrong.ppm` — YUV 当 RGB 的错配
- `out/decode_with_wrong_std.ppm` — 601 编 709 解
- `out/y_plane.pgm` / `out/y_eq.pgm` / 直方图
