# lab14-sampling

对应书中第 14 章：采样。

## 学习目标

- 均匀随机 / 网格 / 分层 / 抖动网格采样
- 估计量方差 `Var(Î)` 的测量与对比——分层采样意义的直接体现
- 二维点集可视化

## 关键公式

- 目标积分：`I = ∫∫ x·y dxdy = 1/4`，估计 `Î = (1/n)Σ f(xᵢ, yᵢ)`。
- 估计量方差用 B 个独立批次的批均值估计：`Var(Î) ≈ (1/B) Σ_b (Î_b − Ī)²`（本 lab B=64，n=4096）。
- grid 用格点中心是确定性求积 → 方差为 0；stratified/jittered 每格一样本，理论 `Var ≈ 1/(24N⁴)`，远小于 random 的 `(7/144)/N²`。
- 常见误区：对样本池直接算 `E[v²]−E[v]²` 无法区分 grid 与 random（对 `f=x·y` 两者都趋近 7/144），必须看估计量方差。

## 编译运行

```powershell
mingw32-make -C labs/lab14-sampling run
```

输出两图：

- `out/lab14_sampling.ppm`：四格点集（random / grid / stratified / jittered grid）。
- `out/lab14_variance.ppm`：四种方法的估计量方差条形图（按最大值归一化，黄=随机、绿=网格、蓝=分层、橙=抖动；网格方差为 0 故无条）。

预期现象：点集图上 grid 明显均匀、random 有疏密团块；条形图上 random 一枝独秀，stratified/jittered 只有 1 像素高的细条（约低 3 个数量级）。

## 建议改动

- `N`（`src/main.c` 宏，默认 64）：每边采样数，N² 为每批样本数；`BATCHES`（默认 64）为方差估计批数。
- `--out`：主输出路径，方差图自动在其文件名后缀前插 `_variance`。
- RNG 种子固定为 9（自检用 5），改 `src/main.c` 中 `cgl_rng_seed` 观察不同随机实现。

## 自检

```powershell
mingw32-make -C labs/lab14-sampling test
```

覆盖：random 估计量方差为正、grid 确定性（方差为 0）、stratified 方差小于 random、random 估计值接近 1/4。

## 常见坑

- 低差异序列（Halton/Sobol）按设计文档明确跳过，本 lab 只覆盖随机类方法。
- 方差条形图按当前最大值归一化，改 N 或 BATCHES 后绝对高度会变，只看相对关系。
