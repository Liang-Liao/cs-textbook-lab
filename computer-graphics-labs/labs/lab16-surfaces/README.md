# lab16-surfaces

对应书中第 16 章：曲面（Bezier patch）。

## 学习目标

- 双三次 Bezier 曲面求值
- 有限差分法向
- 把曲面渲染到图像（UV 密集采样 splat 近似，非解析光线求交）

## 编译运行

```powershell
mingw32-make -C labs/lab16-surfaces run
```

输出：`out/lab16_surfaces.ppm`

渲染方式说明：用 49×49 的 UV 采样点做「点到光线最近距离」splat 近似渲染（教学实现，非解析求交，掠射角会有破洞与厚度感）；细分曲面按设计文档明确跳过。

## 自检

```powershell
mingw32-make -C labs/lab16-surfaces test
```
