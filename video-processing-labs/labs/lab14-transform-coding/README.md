# Lab 14 — 变换、量化与熵编码

8×8 DCT、量化、Zigzag、游程编码（RLE）、Huffman。

## 知识点

1. **8×8 DCT**：能量集中到低频；IDCT∘DCT 相对误差 < 1e-6。
2. **均匀量化**：`q = round(coef/QP)`，QP 越大压缩越狠、失真越大。
3. **感知量化矩阵**：JPEG 亮度表按 quality 缩放；同量非零系数下比平坦量化高约 3–4 dB（q50: 46.3 vs 42.5）——人眼对高频不敏感的加权即"噪声整形"。
4. **Zigzag**：按频率从低到高重排，使高频零值连成片。
5. **RLE**：DC 后按 `(run, level)* EOB` 编码，run 为零游程；对稀疏系数极有效。
6. **Huffman**：对 RLE 符号流做熵编码；稀疏频率表头（符号 int16 + 频次 uint32），解码严格无损。频次用 uint32 —— uint16 会在大流上静默截断、破坏无损性。
7. **QP 扫描**：打印各 QP 的码字大小、压缩比与重建 PSNR，观察 RD 权衡；QP=24 重建图（`out/rec_blocky_qp24.pgm`）展示块效应。

## 运行

```bash
make && make test
```

## 过关判据

| 指标 | 判据 |
|------|------|
| IDCT∘DCT | 相对误差 < 1e-6 |
| Huffman | 编解码严格无损 |
| RLE+Huffman 系数流 | 往返无损 |
| 压缩比（默认 QP=12） | > 2:1 |
| QP 扫描 | ≥3 个 QP，打印 bits/size 与重建误差；QP=24 落盘块效应图 |
| 感知量化矩阵 | quality 10/50/90 三点对比平坦量化（打印 nz/PSNR）；q50 矩阵版 PSNR > 25 dB |

## 产物

- `out/src.pgm` `out/rec.pgm` `out/rec_blocky_qp24.pgm`
- `out/rec_matrix_q50.pgm` `out/rec_blocky_flat.pgm`
