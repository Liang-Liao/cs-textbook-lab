# D1-interp-spline 实验报告

## 环境

- 编译: gcc -std=c11 -O2 (MSYS2 UCRT64)
- 库: mlab 0.1.0 — interp（Lagrange/Newton、分段线性、Hermite、自然三次样条、RBF/TPS、1D kriging）
- vendor: A1(vec/mat/linalg) + A2(dist/stats/gof) + C1(rng) + C9(gp，kriging 同源对照用)
- seed: poly/random 71001；Thomas 对账 72002；RBF 扫描 73003；TPS 73004；仿射再生 73005；加密残差 73006；kriging 73007/73008

## 实验与判据

| 判据 ID | 条件（路线图） | 实测 | 结果 |
|---|---|---|---|
| E1-runage | 高次多项式 Runge 爆炸，显著劣于分段/样条 | n=21 maxerr poly=**59.82**，pwl=0.042，spline=**0.0032**（poly/spline≈1.9e4） | PASS |
| E2-spline-order | 样条实测收敛阶 **4 ± 0.3** | log-log \|slope\| = **4.006**；f=sin(πx)，自然边界 | PASS |
| E3-spline-exact | 节点插值精确 + 自然 BC | max\|s(x_i)-y_i\|=**1.1e-16**；M0=M_end=0 | PASS |
| E4-rbf-shape | 最优 ε 区插值残差 **< 1e-6**；区域外病态可复现 | 最优 ε≈0.75 节点残差 **3.5e-13**；小 ε cond=**8.5e18** vs 最优 6.3e9 | PASS |
| E5-thomas-lu | Thomas 与 A1 LU 一致 | max\|Δ\|=**0**（n=8） | PASS |
| E6-rbf-dense | 区域内插值残差 < 1e-6（**加密散点口径**，:608） | ε=1.0、n=**900** 节点、3000 密集点：max residual=**1.8e-7** | PASS |
| E7-kriging-gp | kriging 与 C9 GP 同源一致（:599） | max\|krig−gp_mean\|=**6.7e-15**（200 点，nugget=1e-2）；nugget=1e-10 节点残差 **2.1e-10** | PASS |

补充对照（非路线图硬锚点，均 PASS）：

- Newton 与 Lagrange 随机节点一致：max\|L-N\|=2.8e-15
- Hermite 节点值/导：节点误差 0；差分导 max=1.0e-10
- TPS（含一次多项式增广）节点残差=1.1e-15；增广再生性：仿射函数 400 密集点残差 **3.1e-15**
- 重复节点 fit 拒收：TPS/Gaussian 均 rc=−2
- 扁核 vs 中等核条件数：6.2e17 vs 1.2e2

## 关键数据

- 样条收敛: `results/spline_convergence.csv`
- RBF 形状扫描: `results/rbf_shape_scan.csv`
- RBF 热图（两行：log10 网格误差 / log10 条件数）: `results/rbf_shape_scan.pgm`
- 加密区域残差: `results/rbf_dense_region.csv`
- kriging–GP 对照: `results/kriging_gp_agree.csv`

## 结论与备注

- **Runge 现象**：等距高次多项式在端点附近误差可达数十量级；同节点自然样条 / 分段线性误差小 2–4 个数量级，实证「高次不如分段」。
- **自然样条 O(h⁴)**：在 f'' 端点为零的 sin(πx) 上，中点最大误差随 h 的 log-log 斜率为 4.006，落在 4±0.3。一般函数若端点二阶导非零，自然边界会引入 O(h²) 主导项；本实验选用端点条件匹配的测试函数以隔离样条本身阶。
- **RBF 形状参数（双口径）**：扫描实验（36 节点）以节点插值残差为口径，最优 ε 区达机器精度；路线图 :608 的「区域内插值残差 < 1e-6」进一步用 30×30=900 节点 + 3000 个密集随机验证点独立验证（1.8e-7）——该口径衡量的是插值体对整个区域的逼近，节点数是决定因素（36 节点时区域残差 ~3e-3，900 节点压到 1e-7 以下），形状参数决定的是可解性窗口。小 ε 病态化（cond 8.5e18）随扫描可复现。
- **TPS 多项式增广**：薄板样条核仅条件正定，增广 degree-1 多项式 [1,x,y] 后经鞍点系统求解；节点残差 1.1e-15，且插值体精确再生一次多项式（3.1e-15），证实增广项生效。重复节点使核矩阵奇异，fit 现以 rc=−2 早拒。
- **kriging 与 GP 同源**：同一高斯协方差（=RBF 核）+ 同一 nugget 下，简单克里金均值与 C9 GP 后验均值在不同求解路径（LU vs Cholesky）上逐点一致至 6.7e-15；nugget→1e-10 时克里金精确过节点（2.1e-10）。高斯核矩阵条件数随节点数超指数增长，双精度下可验证插值性的窗口为小样本（n≈6, ls≈0.3）。
- **基础设施**：Thomas 三对角与 A1 LU 完全一致，样条求解走自实现 Thomas；RBF 核矩阵条件数复用 A1 `mlab_cond2_estimate`；`mlab_cspline_fit` 步长数组 malloc 判空补齐。
- common 增量: `mlab_lagrange_eval`、`mlab_newton_*`、`mlab_piecewise_linear_eval`、`mlab_cubic_hermite_eval`、`mlab_cspline_*`、`mlab_thomas`、`mlab_rbf_*`（TPS 多项式增广 + 重复节点拒收）、`mlab_kriging1d_*`、`mlab_write_pgm_grid`。

## 总判定

- PASS: 16 / FAIL: 0 — **PASS**
