# lab09-shading

对应书中第 9 章：着色（Lambert / Phong / Blinn-Phong）。

## 学习目标

- 漫反射、高光、环境光
- Phong 与 Blinn-Phong 高光差异

## 场景

一排 5 个材质球：环境光 → Lambert → Phong → Blinn-Phong → 金属感混合。

## 范围与简化（明确未实现）

- 所有球都是解析球面法线，**没有三角形网格的法线插值**（flat/smooth shading 对比）；本章设计目标中的「法线插值概念」见 lab20/lab22 的重心插值。
- **没有完整 BRDF** 框架（microfacet GGX 见 lab18）。

## 编译运行

```powershell
mingw32-make -C labs/lab09-shading run
```

输出：`out/lab09_shading.ppm`

## 建议改动

- 调 `shininess` 观察高光锐利度
- 对比 Blinn 的 `half` 向量与 Phong 反射向量

## 自检

```powershell
mingw32-make -C labs/lab09-shading test
```
