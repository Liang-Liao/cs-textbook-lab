# lab13-more-ray-tracing

对应书中第 13/14 章衔接：路径追踪（Monte Carlo 直接光采样 + 俄罗斯轮盘赌）。

## 学习目标

- Whitted 风格递归 vs 随机路径追踪
- 面光源直接采样降低方差
- spp（samples per pixel）与噪声/收敛的关系
- 固定种子可复现

## 编译运行

```powershell
# 默认 32 spp，可改
mingw32-make -C labs/lab13-more-ray-tracing run

# 高 spp 更干净（更慢）
out/bin/lab13 --width 320 --height 180 --spp 128 --seed 1 --out out/lab13_spp128.ppm
out/bin/lab13 --width 320 --height 180 --spp 4  --seed 1 --out out/lab13_spp4.ppm
```

## 场景

- 灰色地板与后墙（漫反射）
- 红/蓝/黄球
- 发光矩形面光源

> 说明：本 lab 使用 NEE（面光源直接采样）+ 漫反射路径弹射，**未做 MIS**。
> 若弹射光线再次打到光源会再加一次 emission，属于教学简化，能量会略偏亮。
> 可作为练习：只对弹射加 emission，或对直接光路径禁用二次光源命中。

## 自检

```powershell
mingw32-make -C labs/lab13-more-ray-tracing test
```
