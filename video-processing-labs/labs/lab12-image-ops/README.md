# Lab 12 — 图像处理与质量评价

对应路线图第 12 章。PPM/PGM、2D 可分离卷积、Sobel、双线性/最近邻 2× 放大、SSIM 滑窗，以及 SSIM 与 PSNR 排序分歧构造。

## 知识点

1. 可分离卷积：2D 高斯 = 水平 1D × 垂直 1D，复杂度从 O(k²) 降到 O(2k)。自测打印 direct / separable 耗时（ms）并断言 rel max err < 1e-9。
2. 边界用 replicate padding，保证直接法与两趟法逐点一致。
3. Sobel 幅值需归一化到 0..255 再写 PGM。
4. 双线性采样在整数格点上精确还原原像素（`sx = dx/2`）；最近邻 2× 在整数格点同样精确，但中间像素为块状复制。
5. SSIM 分离亮度/对比度/结构；纯增益主要动亮度项，结构相关仍高。
6. 亮度平移 MSE 大但结构完好；棋盘结构被 1px 梳状错位破坏 → PSNR 与 SSIM 排序相反。

## 运行

```bash
make && make test
make run
```

## 过关判据

| 指标 | 判据 |
|------|------|
| PPM/PGM 读写器 | 往返逐字节一致；尺寸探测、坏 magic 拒绝、缺文件拒绝（读写器正式化定版） |
| 可分离 vs 直接 2D | rel max err < 1e-9，并打印两侧 ms |
| 双线性 2× 整数格点 | max abs ≤ 0 |
| 最近邻 2× 整数格点 | 精确等于源像素 |
| 纯增益 SSIM | ≥ 0.85（同时打印 PSNR） |
| 排序分歧 | PSNR 偏好 warp，SSIM 偏好 brightness |

## 产物

- `out/io_test.pgm` / `out/io_test.ppm` — 读写器往返测试件
- `out/checker.pgm` `out/circle.pgm` `out/gradient.pgm`
- `out/conv_direct.pgm` `out/conv_sep.pgm`
- `out/sobel.pgm` `out/resize2x.pgm` `out/resize2x_nn.pgm`
- `out/ref.pgm` `out/bright.pgm` `out/warp.pgm`
