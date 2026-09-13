# lab18-materials

对应书中第 18 章：材质（microfacet GGX 入门；理想镜面/玻璃见 lab10）。

## 学习目标

- 漫反射 vs 简化 GGX microfacet 高光
- 粗糙度（alpha）对比
- 与 lab10 折射/Fresnel 的分工：本 lab 聚焦 BRDF 高光

## 编译运行

```powershell
mingw32-make -C labs/lab18-materials run
```

输出：`out/lab18_materials.ppm`

## 自检

```powershell
mingw32-make -C labs/lab18-materials test
```
