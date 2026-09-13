# lab03-ray-tracing

对应书中第 3 章：最小光线追踪器。

## 学习目标

- 针孔相机与主光线
- 球体解析求交
- Lambert 着色、阴影射线、一次反射

## 场景

- 左：红色球（漫反射 + 阴影）
- 右：蓝色球（含反射）
- 底：灰色地平面
- 上：方向光（平行光，非点光源）

## 编译运行

```powershell
mingw32-make -C labs/lab03-ray-tracing run
```

输出：`out/lab03_raytrace.ppm`

## 建议改动

- 调整 `light_dir` / 球心与半径
- 打开/关闭阴影与反射，对比图像
- 增加第二次反射弹射

## 自检

```powershell
mingw32-make -C labs/lab03-ray-tracing test
```

自检验证：射线与单位球在已知距离处求交正确。
