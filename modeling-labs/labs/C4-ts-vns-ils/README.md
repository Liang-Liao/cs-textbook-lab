# C4 禁忌搜索与变邻域/迭代局部搜索

TS / VNS(VND) / ILS 三类单解元启发式，TSP 上与 SA 横向对账。

## 知识点

- 禁忌表与任期；反向移动禁忌；特赦（改进全局最优时无视禁忌）；
  全禁忌随机逃逸（更新 f_best/禁忌表）；长期记忆频次惩罚
- best-improvement vs first-improvement 的取舍（成本/质量实测）
- VNS：shake(k) + 局部搜索，kmax 参数化；邻域族 swap / 2-opt / or-opt
- VND：多邻域轮换下降（按价值排序 2-opt→or-opt→swap）
- ILS：扰动 + 局部搜索；扰动强度 U 形；vs 随机多重启对照
- LNS 毁灭-重建（最小代价重插）与 GRASP（RCL 构造+局部搜索）注记项落地
- 与 SA 对照：概率接受 vs 记忆/邻域系统化/扰动

## 判据

| ID | 实验 | 判据 |
|---|---|---|
| E1 | TS 禁忌长度 | U 形，最优区间两次种子可复现 |
| E2 | VNS 邻域族 | multi 显著优于任一单邻域（vnd=1 去混杂），p<0.01 |
| E3 | ILS 扰动强度 | U 形（s=0 对照与过强均劣于中等强度） |
| E4 | 四机制对账 | SA/TS/VNS/ILS 同预算（±12%）≥100 次表完整 |
| E5 | TS fallback | 全禁忌逃逸计入禁忌表且 f_best 持续改善 |
| E6 | TS 长期记忆 | 频次惩罚生效（n_freq_picks>0）且不劣化 |
| E7 | best/first | 两者都停在（穷举验证的）局部最优；成本对照落盘 |
| E8 | ILS vs 重启 | 同预算 ILS 优于随机多重启局部搜索，p<0.01 |
| E9 | LNS（注记） | 同预算下毁灭-重建超越局部最优（vs 继续 VND） |
| E10 | GRASP（注记） | RCL 最优松紧落在纯贪心与纯随机之间 |

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | tabu、vns_ils（VNS/VND/ILS/best-improve）、grasp_lns（LNS/GRASP） |
| `test/` | harness + test_main |
| `vendor/` | A2 + C1(rng) + C3(sa/tsp，含 or-opt 与距离矩阵) |

测试 suite：tabu、vns、ils、lns、grasp、compare

```bat
mingw32-make check-C4
bin\test.exe --list
bin\test.exe
bin\test.exe <suite>
bin\test.exe <suite>/<case>
mingw32-make -C labs/C4-ts-vns-ils check TEST_ARGS=<suite|suite/case>
```
