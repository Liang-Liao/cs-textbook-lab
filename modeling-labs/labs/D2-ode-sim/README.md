# D2 ODE 数值解与仿真模型

产出仿真内核：Euler / 隐式 Euler / RK4 / RKF45 自适应，以及谐振子、Lotka-Volterra、SEIR、Robertson、药代动力学房室模板与参数敏感性。

## 知识点

- 显式 Euler、隐式 Euler（Newton，不收敛显式返回码）；RK4（Taylor 系数匹配）
- 全局截断误差阶；步长减半误差验证
- 刚性现象：显式法步长被稳定性锁死 vs 隐式法
- 自适应步长：RKF45 嵌入对与局部误差估计；单步 API + 每步 trace（步长曲线）
- 参数敏感性：中心有限差分（为 M3 可辨识性备料）
- 仿真模板：谐振子、LV、SEIR、Robertson、**药代动力学房室（一室 IV + 二室带吸收，含解析解）**

## 判据

| ID | 实验 | 判据 |
|---|---|---|
| E1 | RK4 阶 | 全局误差 log-log 斜率 **4 ± 0.3** |
| E2 | 能量漂移 | 谐振子 RK4 相对漂移 **< 1e-6**，Euler 显著更大 |
| E3 | 自适应误差（去自证） | RKF45 **100 随机初值**真实局部误差（对精确解）**≤ 2·tol** |
| E4 | 刚性 | Robertson：显式在大 h 失败；隐式稳定且质量守恒 |
| E5 | 模板/敏感性 | SEIR 人口守恒；FD 敏感性对账解析 |
| E6 | 隐式 Euler | 一阶收敛（1±0.15）；刚性标量 λ=1000 大步长稳定 |
| E7 | 药代模板 | 一室 IV / 二室带吸收数值解对照解析解（<1e-6） |

路线图原文锚点：

- RK4 实测全局截断误差阶 4 ± 0.3
- 谐振子给定长时程内 RK4 能量相对漂移 < 1e-6（同预算下 Euler 漂移显著更大）
- 自适应求解器在给定容差 tol 下最大局部误差 ≤ 2·tol（100 个随机初值）
- 标准仿真模板：谐振子、捕食-猎物（Lotka-Volterra）、SEIR 传染病、药代动力学房室模型

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | ode（积分器 + 模板（含药代） + 敏感性） |
| `test/` | harness + test_main |
| `vendor/` | A1(vec/mat/linalg) + A2(dist/stats/gof) + B3(lsq) + C1(rng) |

测试 suite：rk4、energy、adaptive、implicit、pk、stiff、templates、sensitivity

```bat
mingw32-make check-D2
bin\test.exe --list
bin\test.exe
bin\test.exe rk4
bin\test.exe energy/harmonic_rk4_drift_vs_euler
bin\test.exe adaptive/rkf45_local_err_le_2tol
bin\test.exe pk
mingw32-make -C labs/D2-ode-sim check TEST_ARGS=<suite|suite/case>
```
