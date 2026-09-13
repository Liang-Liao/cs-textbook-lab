# lab06-viewing

对应书中第 6 章：观察与投影（look-at、透视/正交）。

## 学习目标

- 相机坐标系：forward / right / up
- look-at 矩阵把世界点变换到相机空间
- 透视除法与 FOV 对成像的影响
- 正交 vs 透视

## 编译运行

```powershell
mingw32-make -C labs/lab06-viewing run
```

输出：

- `out/lab06_view_0_persp.ppm` — 50° FOV 透视
- `out/lab06_view_1_wide.ppm` — 90° FOV
- `out/lab06_view_2_ortho.ppm` — 正交投影

## 自检

```powershell
mingw32-make -C labs/lab06-viewing test
```

验证 look-at 后相机前方点落在 -Z 轴附近，原点映射到相机位置。
