# C5 遗传算法与差分进化

二进制 GA（OneMax/解耦函数）与 DE（rand/1、best/1）；F×CR 网格与多样性诊断。

## 知识点

- GA：轮盘/锦标赛、单点/均匀交叉、变异、精英（真 top-k 去重保留）；陷阱函数与早熟
- 种群多样性（位熵 / 平均两两距离）
- DE 记号：rand/1、best/1、current-to-best/1；F、CR；父代适应度缓存（每代只评 trial，n_eval = pop×(max_gen+1)）
- DE/best/1 单峰+大 F 下利用强、多样性塌缩快；DE/rand/1 小 F 稳健探索、多峰更稳；
  Rosenbrock 弯曲谷惩罚贪婪 best/1
- jDE/SaDE、EDA、跨熵、Memetic 为路线图注记项

## 判据

| ID | 实验 | 判据 |
|---|---|---|
| E1 | GA OneMax vs deceptive | OneMax 高比例收敛；deceptive 早熟成功率显著更低 |
| E2 | GA 多样性跌落时刻 | 早熟案例多样性跌落时刻中位数显著早于成功案例（:480） |
| E3 | DE rand vs best 优势反转 | 单峰 Sphere（F=0.7）best/1 终态更优 p<0.01；多峰 Rastrigin（F=0.3）rand/1 终态更优 p<0.01（:479） |
| E3b | Rosenbrock 谷地陷阱 | 形式单峰弯曲谷上 rand/1 终态显著优于贪婪 best/1（p<0.01） |
| E4 | F×CR 网格 | 10D Rastrigin 成功率 5×6 全表 + PGM，每格 50 次 |
| E5 | DE 多样性 | best/1 首次低多样性代数显著早于 rand/1 |

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | ga、de |
| `test/` | harness + test_main |
| `vendor/` | A2 + C1(rng) + C3(bench_sa Rastrigin) + B2(bench Rosenbrock/sphere) |

测试 suite：ga、de

```bat
mingw32-make check-C5
bin\test.exe --list
bin\test.exe
bin\test.exe <suite>
bin\test.exe <suite>/<case>
mingw32-make -C labs/C5-ga-de check TEST_ARGS=<suite|suite/case>
```
