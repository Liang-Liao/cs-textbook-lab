# lab07-texture-mapping

对应书中第 7 章：纹理映射与采样。

## 学习目标

- 球面 UV 参数化
- 最近邻 vs 双线性纹理过滤
- 程序化棋盘纹理（本 lab 不做 mipmap/LOD，可作扩展练习）

## 编译运行

```powershell
mingw32-make -C labs/lab07-texture-mapping run
```

输出 `out/lab07_texture.ppm`：左右对比最近邻 / 双线性。

## 自检

```powershell
mingw32-make -C labs/lab07-texture-mapping test
```
