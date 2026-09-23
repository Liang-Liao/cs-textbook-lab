# 算法选择参考表（M2 本轮实测）

预算 budget=900 评估；每格 100 次独立运行；成功判据 f≤target。

| 问题特征 | 推荐算法 | 依据（本轮数据） |
|---|---|---|
| sphere n=2（单峰/弱多峰，target=1e-04） | **PSO** | rate=100% AES_success=352 |
| sphere n=5（单峰/弱多峰，target=1e-04） | **PSO** | rate=100% AES_success=630 |
| sphere n=10（单峰/弱多峰，target=1e-04） | **PSO** | rate=45% AES_success=877 |
| rosenbrock n=2（单峰/弱多峰，target=1e-02） | **DE** | rate=93% AES_success=900 |
| rosenbrock n=5（单峰/弱多峰，target=1e-02） | 无（全部单层 0%） | 见扩展抽样与收敛曲线；需更大预算或 HYBRID |
| rosenbrock n=10（单峰/弱多峰，target=1e-02） | 无（全部单层 0%） | 见扩展抽样与收敛曲线；需更大预算或 HYBRID |
| rastrigin n=2（多峰，target=5e-02） | **PSO** | rate=83% AES_success=493 |
| rastrigin n=5（多峰，target=5e-02） | 无（全部单层 0%） | 见扩展抽样与收敛曲线；需更大预算或 HYBRID |
| rastrigin n=10（多峰，target=5e-02） | 无（全部单层 0%） | 见扩展抽样与收敛曲线；需更大预算或 HYBRID |
| griewank n=2（多峰，target=5e-02） | **PSO** | rate=100% AES_success=62 |
| griewank n=5（多峰，target=5e-02） | **GA** | rate=98% AES_success=900 |
| griewank n=10（多峰，target=5e-02） | **GA** | rate=98% AES_success=900 |
| ackley n=2（多峰，target=5e-02） | **PSO** | rate=100% AES_success=331 |
| ackley n=5（多峰，target=5e-02） | **PSO** | rate=100% AES_success=582 |
| ackley n=10（多峰，target=5e-02） | **PSO** | rate=80% AES_success=801 |

## 扩展抽样（n=5，各 30 次，budget=900，等预算口径）

| 函数 | HYBRID | CMA-ES | 主表最优单层 |
|---|---|---|---|
| Rosenbrock | 0% | 0% | 无（全部 0%） |
| Rastrigin | 97% | 0% | 无（全部 0%） |

> 尾注以实测为准：精度要求高的场景是否优先 HYBRID/CMA-ES，以上表实测成功率相对主表最优单层的差距为据，实测不占优的格子不予推荐；多峰全局搜索优先 DE/PSO，按需加局部精修。
