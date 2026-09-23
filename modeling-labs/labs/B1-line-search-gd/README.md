# B1 一维搜索与梯度法

黄金分割、Armijo、强 Wolfe、最速下降与条件数敏感性。

## 知识点

- 黄金分割区间收缩（收缩比 ≈ φ−1 = 0.618）与成功抛物线插值思想
- Armijo 回退（c1 充分下降）；强 Wolfe 条件（Armijo + 曲率，bracket+zoom，路线图 L178）
- 病态二次 GD 迭代数放大与锯齿轨迹

## 判据（路线图 L187–189 原文）

| ID | 实验 | 判据（原文） | 实测 |
|---|---|---|---|
| E1 | 黄金分割收缩比 | 黄金分割实测收缩比与 0.618 相对误差 < 1% | 0.61803399，rel=1.5e-10 PASS |
| E2 | Armijo | 可接受下降步；更严 c1 不更少评估；拒绝非下降方向 | alpha=0.25；拒绝上升方向 PASS |
| E3 | 病态 GD | 病态/良态问题迭代数之比与条件数理论预测同数量级 | 2122/58=36.6 ≈ κ 比 40 PASS |
| E4 | 强 Wolfe | 两条件自洽；越过极小时 zoom 回到可接受区间；拒绝上升方向 | 全部 PASS |

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | linesearch / opt |
| `test/` | harness + test_main（多场景 suite/case） |
| `vendor/` | A1（vec/mat/linalg） |
| `results/` | 全量运行自动重算落盘（golden / armijo / gd_traj） |

测试 suite：linesearch、wolfe、steepest_descent

```bat
mingw32-make check-B1
bin\test.exe --list
bin\test.exe
bin\test.exe <suite>
bin\test.exe <suite>/<case>
mingw32-make -C labs/B1-line-search-gd check TEST_ARGS=<suite|suite/case>
```
