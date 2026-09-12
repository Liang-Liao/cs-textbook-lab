# 算法导论 C 实验室

对照《算法导论》第三版（CLRS）逐章实现核心算法的 C 语言实验仓库。每一章一个 lab，可独立编译与测试。

## 快速开始

环境要求（本机已具备）：

- GCC（MSYS2 MinGW-w64，C11）
- `mingw32-make`（或兼容的 `make`）

```powershell
# 在仓库根目录
mingw32-make            # 构建全部 labs
mingw32-make test       # 运行全部测试
mingw32-make ch02       # 只构建第 2 章
mingw32-make test-ch02  # 只跑第 2 章测试
mingw32-make clean
```

也可使用脚本：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Test
```

## 目录说明

| 路径 | 说明 |
|------|------|
| `common/` | 公共头文件与工具：数组、计时、轻量测试 |
| `labs/chNN_*` | 按章划分的实验；每个 lab 自含算法源码、测试、README |
| `DESIGN.md` | 完整设计规范与全书规划 |
| `Makefile` | 顶层构建入口 |

## 学习路线

1. **Phase 1 核心**：第 2–13、15、22–24 章（排序 / 分治 / 数据结构 / 贪心 / 图）
2. **Phase 2 进阶**：第 14、16–19、21、25–26 章（DP / 摊还 / 树与堆 / 并查集 / 全源与最大流）
3. **Phase 3 专题**：第 20、28、30–35 章（vEB、矩阵、FFT、数论、字符串、几何、NP、近似）

完整清单见上方一览表与 [DESIGN.md](DESIGN.md)。

## 已完成 Lab 一览

| 章 | 目录 | 主要内容 |
|----|------|----------|
| 2 | `ch02_getting_started` | 插入排序、归并排序 |
| 3 | `ch03_growth` | 渐近增长对照 |
| 4 | `ch04_divide_and_conquer` | 最大子数组、矩阵乘法、Strassen |
| 5 | `ch05_probabilistic` | 雇佣问题、随机排列 |
| 6 | `ch06_heapsort` | 堆、堆排序、优先队列 |
| 7 | `ch07_quicksort` | Lomuto / 随机化 / Hoare 快排 |
| 8 | `ch08_linear_sort` | 计数、基数、桶排序 |
| 9 | `ch09_order_statistics` | 选择、最坏线性 SELECT |
| 10 | `ch10_elementary_ds` | 栈、队列、链表、树遍历 |
| 11 | `ch11_hash_tables` | 直接寻址、链式、开放寻址 |
| 12 | `ch12_bst` | 二叉搜索树 |
| 13 | `ch13_red_black_trees` | 红黑树 |
| 14 | `ch14_dynamic_programming` | 钢条、矩阵链、LCS、最优 BST |
| 15 | `ch15_greedy` | 活动选择、分数背包、Huffman |
| 16 | `ch16_amortized` | 动态表 |
| 17 | `ch17_augmentation` | 红黑顺序统计树、区间树 |
| 18 | `ch18_b_trees` | B 树 |
| 19 | `ch19_fibonacci_heaps` | 斐波那契堆 |
| 20 | `ch20_van_emde_boas` | vEB 树 |
| 21 | `ch21_disjoint_sets` | 并查集 |
| 22 | `ch22_elementary_graph` | BFS、DFS、拓扑、SCC |
| 23 | `ch23_mst` | Kruskal、Prim |
| 24 | `ch24_shortest_paths` | BF、DAG、Dijkstra |
| 25 | `ch25_all_pairs` | Floyd-Warshall、Johnson |
| 26 | `ch26_maxflow` | Edmonds-Karp |
| 28 | `ch28_matrix_operations` | LUP、行列式、求逆 |
| 30 | `ch30_polynomials_fft` | FFT 多项式乘 |
| 31 | `ch31_number_theory` | 模幂、扩展欧几里得、CRT |
| 32 | `ch32_string_matching` | 朴素、RK、KMP |
| 33 | `ch33_computational_geometry` | 线段相交、凸包 |
| 34 | `ch34_np_completeness` | VC/IS/Clique/3-SAT 判定 |
| 35 | `ch35_approximation` | 顶点覆盖、度量 TSP |

第 1、27 章为导读，未设代码 lab。

## 单个 Lab 如何阅读

以 `labs/ch02_getting_started` 为例：

```text
labs/ch02_getting_started/
├── README.md              # 对应书中章节、算法说明、运行方式
├── insertion_sort.h/.c    # 算法实现（尽量贴近伪码）
├── insertion_sort_test.c  # 单元测试
├── merge_sort.h/.c
├── merge_sort_test.c
├── demo_sort.c            # 小例子演示
└── bench_sort.c           # 计时对比（timing.h）
```

在 lab 目录内：

```powershell
mingw32-make test
```

## 编码约定

- 排序默认接口：`void xxx_sort_int(int *a, size_t n);`
- 测试可执行文件：退出码 `0` 表示通过
- 实现注释标注书中节号（如 CLRS 2.1）
- C 与伪码差异（下标从 0 等）在源文件或 lab README 中说明

## 进度状态

- [x] 工程骨架与 `common` 库
- [x] Phase 1 / Phase 2 / Phase 3 规划章节（35 个 lab）
- [x] ch14 最优 BST、ch17 红黑顺序统计树、ch22 拓扑环检测
- [x] 排序与 BFS 的 `timing.h` 基准
- [x] 全库审计修复：vEB 重复插入不变量、斐波那契堆级联切断守卫、开放寻址墓碑
  upsert、顺序统计树删除 O(lg n)、公共随机数拒绝采样、图加边容量断言、
  最大流净流量矩阵、ch34 3-SAT→CLIQUE 归约，并补充随机对拍与回归测试

里程碑详见 [DESIGN.md](DESIGN.md) §9。
