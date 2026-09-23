# D1 插值与样条

数据侧基本功：多项式插值、Runge 病态、分段/Hermite、自然三次样条、RBF/TPS 形状参数、1D kriging。

## 知识点

- Lagrange / Newton（差商）多项式插值；高次等距节点的 **Runge 现象**
- 分段线性、三次 Hermite（值 + 一阶导）
- 自然三次样条（M0=M_{n-1}=0）；三对角 Thomas 求解（与 A1 LU 对账）
- RBF：高斯核 φ(r)=exp(-(ε r)²)、薄板样条 r²log r；形状参数的精度-病态权衡
- TPS 多项式增广（degree-1 [1,x,y] 鞍点系统；条件正定核的唯一可解性与仿射再生性）；重复节点拒收
- kriging 与 C9 GP 同源：简单克里金 = 核矩阵线性系统（LU），对照 C9 GP 后验均值（Cholesky）

## 判据

| ID | 实验 | 判据 |
|---|---|---|
| E1 | Runge | n=21 高次多项式 maxerr **显著大于** 分段/样条（≥5×） |
| E2 | 样条收敛阶 | sin(πx) 自然样条 log-log 斜率 **4 ± 0.3** |
| E3 | 样条精确性 | 节点插值残差 < 1e-12；自然 BC |
| E4 | RBF 形状参数 | 最优 ε 区节点残差 **< 1e-6**；小 ε 病态化可复现 |
| E5 | Thomas vs LU | 三对角解与 A1 LU 一致（<1e-10） |
| E6 | 区域残差（加密口径） | 最优 ε、密集节点 + 3000 密集验证点残差 **< 1e-6** |
| E7 | kriging ↔ GP | 同核同 nugget 下预测均值一致（<1e-10）；数值 nugget 过节点 |

路线图原文锚点：

- 样条实测收敛阶 4 ± 0.3
- 最优形状参数区域内插值残差 < 1e-6（光滑测试函数）；区域外病态化现象可复现
- kriging 与 GP 的同源性（对照 C9）

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | interp（多项式/样条/RBF/TPS/kriging） |
| `test/` | harness + test_main |
| `vendor/` | A1(vec/mat/linalg) + A2(dist/stats/gof) + C1(rng) + C9(gp) |

测试 suite：poly、runge、spline、hermite、rbf、kriging

```bat
mingw32-make check-D1
bin\test.exe --list
bin\test.exe
bin\test.exe spline
bin\test.exe spline/convergence_order_4
bin\test.exe rbf/dense_region_residual_1e-6
bin\test.exe kriging
mingw32-make -C labs/D1-interp-spline check TEST_ARGS=<suite|suite/case>
```
