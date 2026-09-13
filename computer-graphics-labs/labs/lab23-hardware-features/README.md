# lab23-hardware-features

对应书中第 23 章：硬件特性（软件模拟混合/模板/多边形偏移）。

## 学习目标

- Alpha 混合：`C = src·a + dst·(1−a)`
- 模板缓冲：每像素一个 mask 值，只有 `stencil==1` 的像素才允许写入
- 多边形偏移（polygon offset）：深度测试前给该图元的片段深度加偏移 `z' = z − bias`，解决共面贴花（decal）的 z-fighting

## 关键公式

- 混合：`C = src·a + dst·(1−a)`
- 多边形偏移：深度测试与写入都用 `z − bias`；wall 与 decal 共面（z 相等）时，无偏移的 decal 在严格 `<` 测试下永远失败，加 bias 后整体被抬到 wall 前面

## 编译运行

```powershell
mingw32-make -C labs/lab23-hardware-features run
```

输出：`out/lab23_features.ppm`（640×320，自上而下三个区域）：

- 上半部：左椭圆 alpha 混合（背景透出）、右椭圆不混合直接替换；
- 中部：绿色横条只出现在左半——左半启用了模板测试（mask 为大圆内 `stencil==1`），右半未启用模板绘制，所以没有绿条；
- 底部条带：同一组共面 wall+decal 三角形，左边 bias=0、右边开偏移（bias=0.02）。

预期现象：右边 wall 上叠着完整的橙色 decal；左边 decal 只在插值舍入恰好偏向它的像素上零星出现（z-fighting 碎斑，与真实 GPU 共面贴花的条纹伪影同源）——这正是多边形偏移要解决的问题。

## 建议改动

- `--bias`（默认 0.02）：偏移量，调大到超过 wall 前后真实深度差时会看到 decal 「穿透」错误遮挡。
- `src/main.c` 中混合系数 `a=0.55`、椭圆尺寸、stencil mask 半径。

## 自检

```powershell
mingw32-make -C labs/lab23-hardware-features test
```

覆盖：混合公式、8×8 小图上「共面 decal 无偏移必败 / 有偏移必过」的深度测试行为。

## 常见坑

- 真实 GPU 上的 z-fighting 因插值舍入表现为条纹/碎斑：本 lab 的左半条带里就可见（共面三角形的每像素 z 由浮点舍入决定胜负）。自检用的 8×8 小图恰好舍入一致，故断言「无偏移必败」在该尺寸下成立。
- 深度测试约定：z 越小越近，测试为严格 `<`。
