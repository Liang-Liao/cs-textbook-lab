# 算法导论 C 实验室 — 设计文档

## 1. 目标

对照《算法导论》第三版（CLRS, Cormen / Leiserson / Rivest / Stein），用 C11 实现书中可编程的核心算法。每一章对应一个独立 lab，便于按阅读进度学习、编译、测试与实验。

## 2. 设计原则

1. **忠于伪码**：实现结构尽量贴近书中伪码；C 与伪码的差异（下标从 0、指针、内存所有权）在注释或 README 中说明。
2. **一章一目录**：章内按算法拆分文件，避免巨型源文件。
3. **先正确再高效**：同一问题可提供朴素版与优化版（如矩阵乘法的三次方与 Strassen）。
4. **可验证**：每个算法配单元测试；重要算法配规模扫描 bench。
5. **中文导读**：每章 README 标注对应书中章节、关键结论与练习建议。

## 3. 目录结构

```
algorithmslab/
├── README.md                 # 总览、构建、学习路线
├── DESIGN.md                 # 本设计文档
├── Makefile                  # 顶层构建入口
├── common/
│   ├── include/
│   │   ├── clrs.h            # 公共类型、比较、错误宏
│   │   ├── array.h           # 数组工具
│   │   ├── timing.h          # 计时
│   │   └── test.h            # 轻量测试框架
│   └── src/
│       ├── array.c
│       ├── timing.c
│       └── test.c
├── labs/
│   └── chNN_short_name/
│       ├── README.md
│       ├── Makefile
│       ├── <algo>.h / <algo>.c
│       ├── <algo>_test.c
│       └── (可选) demo_*.c / bench_*.c
└── scripts/
    └── build.ps1
```

## 4. Lab 编排规范

### 4.1 文件职责

| 文件 | 职责 |
|------|------|
| `algo.h` | 函数签名、复杂度与书中节号注释 |
| `algo.c` | 实现，步骤尽量对应伪码 |
| `algo_test.c` | 边界、随机对照、与标准库交叉验证 |
| `README.md` | 本章导读、算法清单、构建与运行方式 |
| `Makefile` | 编译本章目标并链接 `common` |

### 4.2 命名约定

- 目录：`chNN_short_name`，两位章号，如 `ch02_getting_started`
- 符号：语义清晰的 `snake_case`，如 `insertion_sort_int`、`merge_sort_int`
- 测试：`*_test.c`，可执行文件退出码 0 表示全部通过
- 章内 Makefile 的默认目标：`all`（库/算法对象 + 测试可执行文件）、`test`、`clean`

### 4.3 接口约定

- 整型排序默认接口：`void xxx_sort_int(int *a, size_t n);`
- 比较驱动接口（可选）：`void xxx_sort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *));`
- 图、树等复杂结构：头文件中定义不透明类型或明确结构体，并提供 init/free

## 5. 公共库（common）

| 模块 | 能力 |
|------|------|
| `clrs.h` | `CLRS_UNUSED`、`CLRS_ASSERT`、通用比较函数、内存分配辅助 |
| `array.h` | 随机/顺序/逆序/常数填充，拷贝，有序性检查，打印 |
| `timing.h` | 微秒级计时 `clrs_now_us` / `clrs_elapsed_us` |
| `test.h` | `ASSERT_TRUE` / `ASSERT_EQ_INT` / 断言汇总与 `test_report` |

测试二进制链接 `common` 静态对象；章内不重复实现这些能力。

## 6. 构建系统

- **工具**：MinGW-w64/MSYS2 的 `gcc` + `mingw32-make`（本机已具备）；兼容 `make`。
- **标准/警告**：`-std=c11 -Wall -Wextra -Wpedantic`
- **优化**：默认 `-O2`；测试目标可用 `-O0 -g` 便于调试（通过变量覆盖）。
- **顶层命令**：

  ```text
  make              # 构建全部 labs
  make ch02         # 构建 labs/ch02_*
  make test         # 运行全部 *_test
  make test-ch02    # 运行第 2 章测试
  make clean
  ```

- 顶层 Makefile 通过通配发现 `labs/chNN_*`，避免每新增一章改顶层列表（新增章只需保证目录名符合约定）。

## 7. 全书 Lab 规划

### Phase 1 — 核心

| 章 | 目录名 | 核心算法 |
|----|--------|----------|
| 2 | `ch02_getting_started` | 插入排序、归并排序 |
| 3 | `ch03_growth` | 渐近记号对照（演示/数值） |
| 4 | `ch04_divide_and_conquer` | 最大子数组、矩阵乘法、Strassen |
| 5 | `ch05_probabilistic` | 随机雇佣、随机排列 |
| 6 | `ch06_heapsort` | 堆、堆排序、最大优先队列 |
| 7 | `ch07_quicksort` | 快排、随机化、划分变体 |
| 8 | `ch08_linear_sort` | 计数、基数、桶排序 |
| 9 | `ch09_order_statistics` | 最小最大、期望选择、最坏线性选择 |
| 10 | `ch10_elementary_ds` | 栈、队列、链表、树遍历 |
| 11 | `ch11_hash_tables` | 直接寻址、链式、开放寻址 |
| 12 | `ch12_bst` | BST 插入/删除/遍历/后继前驱 |
| 13 | `ch13_red_black_trees` | 红黑树旋转/插入/删除 |
| 15 | `ch15_greedy` | 活动选择、分数背包、Huffman |
| 22 | `ch22_elementary_graph` | BFS、DFS、拓扑排序、SCC |
| 23 | `ch23_mst` | Kruskal、Prim |
| 24 | `ch24_shortest_paths` | Bellman-Ford、DAG、Dijkstra |

### Phase 2 — 进阶

| 章 | 目录名 | 核心算法 |
|----|--------|----------|
| 14 | `ch14_dynamic_programming` | 钢条切割、矩阵链乘、LCS、最优 BST |
| 16 | `ch16_amortized` | 动态表摊还行为 |
| 17 | `ch17_augmentation` | 顺序统计树、区间树 |
| 18 | `ch18_b_trees` | B 树 |
| 19 | `ch19_fibonacci_heaps` | 斐波那契堆 |
| 21 | `ch21_disjoint_sets` | 并查集 |
| 25 | `ch25_all_pairs` | Floyd-Warshall、Johnson |
| 26 | `ch26_maxflow` | Edmonds-Karp（Ford-Fulkerson + BFS 增广） |

### Phase 3 — 专题（按需）

| 章 | 目录名 | 主题 |
|----|--------|------|
| 20 | `ch20_van_emde_boas` | vEB 树 |
| 28 | `ch28_matrix_operations` | 矩阵运算 |
| 30 | `ch30_polynomials_fft` | 多项式与 FFT |
| 31 | `ch31_number_theory` | 数论算法 |
| 32 | `ch32_string_matching` | 朴素 / Rabin-Karp / KMP |
| 33 | `ch33_computational_geometry` | 线段相交、凸包 |
| 34 | `ch34_np_completeness` | 归约示例 |
| 35 | `ch35_approximation` | 近似算法 |

第 1、27 章以 README 导读为主，不强制实现代码。

### 实现状态

Phase 1–3 规划章节目录均已实现且 `make test` 全部通过。与规划的少量差异：

| 项 | 说明 |
|----|------|
| ch26 | 目录名 `ch26_maxflow`；算法为 Edmonds-Karp（已含 FF 框架 + BFS 增广） |
| ch17 | 顺序统计树已用红黑树 + size 扩张（O(lg n)） |
| ch22 | 拓扑排序含环检测 |
| ch14 | 已含最优 BST |
| bench | ch02 `bench_sort`、ch22 `bench_bfs` 使用 `timing.h` |
| 审计修复 | vEB 重复插入、斐波那契堆级联切断、开放寻址墓碑 upsert、OST 删除
  O(lg n)、公共随机数（拒绝采样）、图加边容量断言、最大流净流量 API、
  ch34 补 3-SAT→CLIQUE 归约；相应测试与文档契约已对齐 |

## 8. 验证策略

1. **单元测试**：空数组、单元素、两元素、已序、逆序、含重复、随机大样本。
2. **交叉验证**：排序结果与 `qsort` 逐元素比对；搜索结果与线性扫描比对。
3. **复杂度实验**：`bench` 对多档 `n` 记录耗时，观察曲线趋势。
4. **失败可见**：`ASSERT_*` 打印文件:行号与期望/实际值，非零退出。

## 9. 实现顺序（里程碑）

| 里程碑 | 内容 | 状态 |
|--------|------|------|
| M0 | 设计文档 + README + common + ch02 模板 + 构建可用 | 已完成 |
| M1 | 完成 Phase 1 其余章节 | 已完成 |
| M2 | 完成 Phase 2（ch14,16–19,21,25–26） | 已完成 |
| Phase 3 | 按学习进度补充（ch20,28,30–35 已完成） | 已完成 |

## 10. 环境

- OS：Windows
- 编译器：MSYS2 UCRT64 gcc（gcc 16.x）
- 构建：`mingw32-make`
- 语言标准：C11
