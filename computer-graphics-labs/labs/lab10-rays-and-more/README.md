# lab10-rays-and-more

对应书中第 10 章：折射 / 景深 / 软阴影入门。

## 学习目标

- Snell 折射与 Fresnel 反射权重
- 薄透镜景深（DOF）
- 面光源软阴影（随机采样光源面积）

## 编译运行

```powershell
mingw32-make -C labs/lab10-rays-and-more run
```

输出：`out/lab10_rays.ppm`（玻璃球 + 景深 + 软阴影）

## 自检

```powershell
mingw32-make -C labs/lab10-rays-and-more test
```
