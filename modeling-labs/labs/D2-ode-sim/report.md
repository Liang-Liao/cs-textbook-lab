# D2-ode-sim 实验报告

## 环境

- 编译: gcc -std=c11 -O2 (MSYS2 UCRT64)
- 库: mlab 0.1.0 — ode（Euler / 隐式 Euler / RK4 / RKF45（单步+trace）/ 仿真模板（谐振子/LV/SEIR/Robertson/药代动力学房室）/ FD 敏感性）
- vendor: A1(vec/mat/linalg) + A2(dist/stats/gof) + B3(lsq) + C1(rng)
- seed: RKF45 随机初值 74001

## 实验与判据

| 判据 ID | 条件（路线图） | 实测 | 结果 |
|---|---|---|---|
| E1-rk4-order | RK4 全局截断误差阶 **4 ± 0.3** | log-log slope = **3.954**；y'=-2ty on [0,1] | PASS |
| E2-energy | 谐振子 RK4 能量相对漂移 **< 1e-6**；Euler 显著更大 | T=40 h=0.02：RK4 drift=**1.78e-9**；Euler drift=**1.23** | PASS |
| E3-adaptive | 自适应局部误差 **≤ 2·tol**（100 随机初值，**对精确解**） | tol=1e-5，max_true_local=**5.27e-6** ≤ 2e-5（内部估计 max 1.76e-5），violations=**0** | PASS |
| E4-stiff | Robertson 显式天花板 vs 隐式稳定 | h=0.05：显式 Euler **NaN**；隐式 y=(0.716,9.2e-6,0.284) sum=1.000000 | PASS |
| E5-templates | SEIR 人口守恒；敏感性 FD 对账 | SEIR Δpop=**9e-12**；∂y/∂θ 与解析 \|Δ\|**=2e-10** | PASS |
| E6-implicit | 隐式 Euler 一阶收敛；刚性好步长稳定（补套件） | order=**0.883**（1±0.15）；λ=1000、h=0.5：显式 **8.4e+107** 发散，隐式 y=**1.000000**（稳态） | PASS |
| E7-pk | 药代动力学房室模板对照解析解（:623 点名缺口） | 一室 IV max\|Δ\|=**1.5e-9**；二室带吸收 max\|Δ\|=**1.2e-9**（A1 峰 4.06 吸收相） | PASS |

## 关键数据

- RK4 阶: `results/rk4_order.csv`
- 谐振子能量: `results/harmonic_energy.csv`
- RKF45 真实局部误差: `results/rkf45_error_bound.csv`（err_est / err_true 双列）
- 步长曲线（实验 3）: `results/rkf45_steps.csv`（harmonic / robertson 两条）
- 隐式 Euler 阶: `results/implicit_euler_order.csv`
- 药代: `results/pk_one_comp.csv`、`results/pk_two_comp.csv`
- Robertson 刚性: `results/robertson_stiff.csv`

## 结论与备注

- **RK4 阶**: 在 y'=-2ty（光滑、解析解 exp(-t²)）上，终点全局误差随 h 的 log-log 斜率为 3.954，落在 4±0.3。
- **能量漂移**: 谐振子非辛 RK4 在 T=40、h=0.02 下相对漂移约 2e-9，满足 <1e-6；同预算显式 Euler 能量膨胀到 O(1)（drift≈1.23），对照鲜明。
- **自适应 RKF45（去自证）**: 修复轮前用求解器内部误差估计对照 2·tol 属自证；现改为对 100 条随机初值轨迹的每个接受步，从**精确解**出发以同一步长单步积分（新增 `mlab_ode_rkf45_step`），与精确解之差即真实局部误差。实测 max 5.27e-6 ≤ 2·tol（内部估计 1.76e-5 系保守上界，量级合理）。步长曲线落盘：光滑谐振子上步长从 0.01 放大到 0.26（26×），Robertson 早期刚性段 h_max 被压到 3.7e-3，"步长-光滑度对应"可直接从 CSV 读出。
- **隐式 Euler（补套件）**: y'=-2y 上实测一阶（0.883）；刚性标量 λ=1000 在 h=0.5（超出显式稳定上限 250 倍）下显式发散至 8.4e+107、隐式单调衰减到稳态 1。Newton 不收敛现显式返回 -3（此前静默接受未收敛状态）；线性求解失败返回 -2。
- **药代动力学房室模板**（路线图 :623 标准仿真模板的点名缺口）: 一室 IV bolus（C'=−kel·C）与二室+一级吸收（depot→central↔peripheral）模板 + 三指数解析解（系数由初值与 ODE 右端在 t=0 的约束经 2×2 线性系统确定，避免退化特判）。RKF45 紧容差下数值解与解析解偏差 ~1e-9；二室曲线呈典型吸收相（A₁ 峰值 4.06 @ t≈1.5）→分布相→消除相，为 M3 房室建模备料。
- **敏感性**: 指数衰减与谐振子对 ω 的 ∂y/∂θ 中心差分与解析式一致，为 M3 参数可辨识性准备接口。
- vendor 中 B3 `lsq` 为路线图依赖占位（M1/M3 拟合复用），本 lab 积分路径未强制调用。
- common 增量: `mlab_ode_euler`、`mlab_ode_rk4`、`mlab_ode_euler_implicit`、`mlab_ode_rkf45`（+`_trace`/`_step`）、`mlab_harm_*`、`mlab_lv_rhs`、`mlab_seir_rhs`、`mlab_robertson_rhs`、`mlab_pk1_*`、`mlab_pk2_*`、`mlab_ode_sensitivity_fd`。

## 总判定

- PASS: 13 / FAIL: 0 — **PASS**
