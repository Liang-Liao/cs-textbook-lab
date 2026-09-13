# lab05-transformation-matrices

对应书中第 5 章：变换矩阵（平移/旋转/缩放/齐次坐标/复合）。

## 学习目标

- 用 4x4 齐次矩阵统一仿射变换
- 复合顺序：先局部旋转再世界平移
- 法向量应用逆转置（各向异性缩放时方向会变）

## 编译运行

```powershell
mingw32-make -C labs/lab05-transformation-matrices run
```

生成 4 张图：

- `out/lab05_xform_0_identity.ppm`
- `out/lab05_xform_1_translate.ppm`
- `out/lab05_xform_2_rotate_scale.ppm`
- `out/lab05_xform_3_composite.ppm`

同一个球体在四种变换矩阵下渲染（环境光 + Lambert 方向光，无阴影射线）。
第 2 张演示平移，第 3 张是各向异性缩放 (1.4, 0.7, 1) + 旋转的复合——球被压成椭球且明暗随之改变，这就是逆转置法线矩阵要处理的情况；第 4 张再叠加平移与绕 y 旋转。

预期现象：对比第 0/2 张，椭球的长轴沿旋转后的 x 方向、亮斑位置与纯缩放不同——说明法线没有直接用 M 的 3x3 部分变换。

## 自检

```powershell
mingw32-make -C labs/lab05-transformation-matrices test
```
