# lab08-data-structures

对应书中第 8 章：图形学数据结构（AABB / 包围盒剔除）。

## 学习目标

- 包围盒 slab 法求交
- AABB 剔除 vs 暴力求交（结果一致性）
- 理解均匀网格分箱（`build_grid` 演示对象划分；主路径当前用 AABB 剔除保证正确性）

## 编译运行

```powershell
mingw32-make -C labs/lab08-data-structures run
```

输出：`out/lab08_accel.ppm` + stdout 打印 brute/aabb 耗时与命中一致性。

## 自检

```powershell
mingw32-make -C labs/lab08-data-structures test
```

## 练习建议

修复/实现真正的均匀网格 DDA 遍历，使加速结构在高密度球场景下快于暴力求交（注意多 cell 的链表插入：`cell_next[i]` 不能每个 cell 覆盖写）。