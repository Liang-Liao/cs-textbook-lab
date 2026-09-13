---
feature: fo-cg-labs-curriculum
status: delivered
updated: 2026-09-13
branch: feat/fo-cg-labs-curriculum
commits: ffcdf28..059fe32
---

# FoCG 第 5 版逐章 C 实验（Computer Graphics Labs）

## Report

**What was built** — 在 `feat/fo-cg-labs-curriculum` 上实现了完整教学型实验套件：`common/` 极薄库（向量/矩阵含 invert、sRGB、PPM 图像、RNG、自检宏）+ 24 个相对独立 C lab（lab00–lab23），覆盖 FoCG 5e 主题从解析图像、最小光追、变换/观察，到纹理、加速、着色、路径追踪、采样、曲线曲面、光源材质、可见性与软件光栅化。每个 lab 含 README、Makefile、可运行 `src`，默认离线写出 PPM。构建统一走 MSYS2 UCRT64 的 `gcc` + `mingw32-make` 与共享 `make/lab.mk`；顶层 Makefile 为纯 Make 规则，PowerShell/cmd/bash 均可。

**Verification** — 在本机 UCRT64 下执行：
- `mingw32-make list`：24 个 lab 均为 ready（PASS，PowerShell）
- `mingw32-make test`：全部 `--self-test` PASS，exit 0（PowerShell，含 FORCE 修复后）
- `mingw32-make all` / `make run LAB=lab00-toolchain-smoke`：exit 0
- `cgl_assert.h` 单独 `-Werror` 可编译；lab05/13 自检 PASS
- 抽样：lab03 写出 PPM；lab13 写出 320x180 spp=32 图；lab22 写出 z-buffer 图；lab08 brute vs aabb mismatch=0

独立评审结论：无 critical；已知教学简化保留在 README（lab13 NEE 无 MIS 的双计、lab08 网格 DDA 留作练习）。审查收口修复见 Review fixes 任务。

**Journey log**
1. 空仓库无提交，先 seed README 再切 feature 分支；本会话禁用 `git worktree add`，直接在 `feat/fo-cg-labs-curriculum` 上交付。
2. `lab.mk` 曾把 `COMMON_DIR` 算成上两级目录，导致找不到 `common`；改为基于 `make/lab.mk` 的 `CGL_ROOT/../common`。
3. lab08 均匀网格 DDA 在非立方包围盒下结果不一致；改为 AABB 剔除保证正确，并把完整网格 DDA 留作练习（README 已说明 `cell_next` 多 cell 覆盖写问题）。
4. 多处自检断言把 reflect 结果误写成法向分量=1；改为验证切向不变、法向取反。
5. 2026-09-12 审查：顶层 Makefile 原 POSIX shell 循环在 PowerShell 下失败 → 改纯 Make + FORCE；lab07 虚假 mipmap 表述删除；`cgl_mat4_invert` 上提 common；lab13 平行轴与 cos_light 修正；写失败路径统一 free+stderr；lab18 README/注释与代码对齐。

## [S1] Problem

学习《Fundamentals of Computer Graphics》第 5 版时，仅阅读公式与插图难以建立直觉。需要一套与章节对应、彼此相对独立、可在 MSYS2 UCRT64 下用 C 语言编译运行的实验（lab），通过写图像、改参数、对照书中现象来巩固概念。

约束与目标：

- 语言：C11
- 环境：Windows + MSYS2 UCRT64（gcc 16.1，`mingw32-make` 位于 MSYS2 安装目录的 `ucrt64\bin`，默认 `C:\msys64\ucrt64\bin`）
- 范式：纯软件渲染（软件光栅化 + 光线/路径追踪），不依赖 OpenGL/GPU
- 产出：每个 lab 编译后运行，写出离线图像（优先 PPM，可选 PNG）
- 覆盖：书中可实现章节全覆盖；纯理论章做轻量验证实验或明确标记为可选/跳过
- 依赖：尽量零依赖；允许 vendored 单头文件库（如 `stb_image_write.h`）用于 PNG
- 交付节奏：先完成本设计文档，人工审核通过后按章节实现

## [S2] Design

### 2.1 风格锚点与学习形态

- 产品锚点：MIT 6.837 / GAMES101 作业式「小而完整」的离线渲染实验，而非游戏引擎。
- 每个 lab = 一章概念的可观察验证：最小可运行程序 + README（对应章节、数学要点、如何改参数）+ 预期输出说明。
- 强调「实现算法本身」，不在 UI、窗口系统、资产管道上耗散精力。

### 2.2 总体架构

```
computer-graphics-labs/
  README.md
  Makefile                 # 顶层：list / all / clean / test
  common/                  # 极薄公共层（仅高频、稳定的基础设施）
    cgl_vec.h              # 向量/矩阵/标量工具（header-only）
    cgl_color.h            # 颜色与 tone-map（sRGB/线性）
    cgl_image.h/.c         # 内存图像 + PPM(P3/P6) 读写
    cgl_png.h              # 可选 PNG（内嵌 stb，无则仅 PPM）
    cgl_rng.h              # 可复现 RNG（xorshift 等）
  vendor/
    stb_image_write.h      # 仅当需要 PNG 时 vendored
  labs/
    chNN-slug/
      README.md
      Makefile
      src/*.c *.h
      expected/            # 可选：参考输出说明或 golden 图（后期）
  docs/
    compose/spec/fo-cg-labs-curriculum.md
    roadmap.md             # 用户可读的章节-实验对照与进度
  tools/
    run_lab.ps1 / run_lab.sh  # 一键编译运行并列出输出路径
```

**相对独立**的含义：

- 每个 `labs/chNN-slug` 可单独 `make` / `make run`，失败不拖垮其他 lab。
- 仅允许依赖 `common/` 的稳定 API；**禁止** lab 之间互相 include。
- 早期 lab 若只需极少量工具，允许在 lab 内自带，待第三处重复再上提进 `common/`（Rule of Three 仅适用于 common 扩张，不阻止 lab 自包含）。
- `common/` 只含与章节无关的基础设施，不含「某章算法」，避免实验变成调库。

### 2.3 构建与工具链

| 项 | 约定 |
|----|------|
| 方言 | `-std=c11 -Wall -Wextra -Werror`（后期若第三方头警告可对 vendor 关闭 Werror） |
| 优化 | 开发默认 `-O0 -g`；`make release` 为 `-O2` |
| 数学库 | 链接 `-lm`（MinGW 需要） |
| Make | UCRT64 的 `mingw32-make`（`usr\bin\make` 为 MSYS make，不作为默认） |
| 路径 | 文档给出把 `C:\msys64\ucrt64\bin`（MSYS2 默认安装路径）加入 PATH 的说明 |
| 输出 | 默认 `out/` 目录（gitignore），图像名如 `out/ch03_raytrace.ppm` |

顶层 Makefile 目标（纯 Make 规则，PowerShell/cmd/bash 均可）：

- `make list` — 列出全部 lab 与状态
- `make -C labs/lab03-ray-tracing` — 单 lab 构建
- `make all` — 顺序构建全部
- `make release` — 全部 `-O2` 重建
- `make run LAB=lab03-ray-tracing` — 构建并运行
- `make clean`

每个 lab Makefile 使用统一的 `lab.mk` 片段（编译器 flags、common 路径、out 目录）。

**正确性验证策略（纯软件、零依赖）：**

1. 确定性：关闭 ASLR 无关性问题；RNG 固定种子。
2. 数值断言：对线性代数、交点、投影等可打印「自检表」（误差阈值内 PASS）。
3. 图像回归（可选、后期）：对简单场景输出 PPM 哈希或误差图；第一期不强制 golden image。
4. `make test` 运行各 lab 的 `--self-test`（若实现）或固定分辨率烟雾运行并检查退出码与输出文件非空。

### 2.4 图像与 I/O 约定

- 内存布局：`float` 线性光 RGB，宽优先 `w * h * 3`；原点在图像**左上**（与多数教材图一致），y 向下；在 README 中写死，避免与书内坐标系混淆时无据可查。
- 写出：默认 PPM P6（二进制）；命令行/宏可选 P3 便于 diff。
- PNG：`CGL_WITH_PNG` 时使用 vendored stb；默认关闭以保持零依赖路径可用。
- 颜色：渲染在线性空间；写 8-bit 前做 sRGB OETF（或可选 gamma 2.2），lab 中显式可选，用于对比「线性 vs 显示编码」。

### 2.5 公共 API 契约（common，首版）

```c
/* cgl_vec.h — header-only */
typedef struct { float x, y; } cgl_vec2;
typedef struct { float x, y, z; } cgl_vec3;
typedef struct { float m[16]; } cgl_mat4; /* column-major, m[col*4+row] */
/* add/sub/mul/div, dot, cross, length, normalize, reflect, refract */
/* mat4 identity/translate/scale/rotate_xyz/perspective/look_at/mul/invert */

/* cgl_image.h */
typedef struct {
  int w, h;
  float *rgb; /* w*h*3, linear */
} cgl_image;
cgl_image *cgl_image_create(int w, int h);
void cgl_image_free(cgl_image *img);
void cgl_image_set(cgl_image *img, int x, int y, cgl_vec3 c);
cgl_vec3 cgl_image_get(const cgl_image *img, int x, int y);
int cgl_image_write_ppm(const cgl_image *img, const char *path, int binary);
/* 可选 PNG：cgl_image_write_png */
/* 实现补充：cgl_image_write_ppm_srgb(img, path) —— 写前做 sRGB OETF 的 P6 快捷方式 */

/* cgl_rng.h */
typedef struct { uint64_t s; } cgl_rng;
void cgl_rng_seed(cgl_rng *r, uint64_t seed);
float cgl_rng_next01(cgl_rng *r);
```

矩阵采用 **column-major**（与数学公式 \(Mv\) 及常见 GL 习惯一致）；lab 文档中写明，并在 Viewing lab 用相机基向量验证。

### 2.6 章节 → Lab 映射（FoCG 5e）

> 章号以第 5 版常见 23 章结构为准。若纸质书/PDF 章号有偏移，以**主题 slug** 为准，lab README 标注「对应书中 §主题」。`Miscellaneous Math` 内容并入 ch04 及各 lab 就近讲解，不单独设 lab。

| Lab | 书章 | Slug | 一句话目标 | 输出/验证 |
|-----|------|------|------------|-----------|
| lab00 | 前置 | toolchain-smoke | 确认 gcc/make、写出第一张 PPM | 纯色/渐变图 + 退出码 |
| lab01 | 1 | introduction | 用代码表达「图像=采样函数」：解析式 2D 图案 | 函数图像 PPM |
| lab02 | 2 | raster-images | 采样、量化、分辨率、简单滤波/缩放 | 多分辨率与 8-bit 量化对比图 |
| lab03 | 3 | ray-tracing | 最小光线追踪：针孔相机、球、平面、Lambert、阴影、反射 | Cornell-like 或经典三球场景 |
| lab04 | 4 | linear-algebra | 向量几何可视化与数值自检（基变换、点积投影） | 自检表 + 向量场/坐标系示意 PPM |
| lab05 | 5 | transformation-matrices | 齐次变换、复合、法变换；渲染变换后的几何 | 动画帧序列（多张 PPM）或变换前后对比 |
| lab06 | 6 | viewing | look-at、透视/正交投影矩阵、视口 | 同一场景不同 FOV/相机位姿对比 |
| lab07 | 7 | texture-mapping | UV 参数化、纹理采样、最近邻/双线性（mipmap 明确跳过） | 纹理球；最近邻 vs 双线性对比 |
| lab08 | 8 | data-structures | AABB、均匀网格或 BVH；加速 lab03 追踪 | 性能计时 + 同图像对比 |
| lab09 | 9 | shading | Phong/Blinn、法线插值概念、更完整 BRDF 入门 | 材质球棚拍（material spheres） |
| lab10 | 10 | rays-and-more | 折射、透镜/景深、软阴影入门（面光源采样雏形） | 玻璃球、DOF、区域光图 |
| lab11 | 11 | color | 线性光与 sRGB、色域、简单颜色空间转换 | 同一渲染经不同编码对比条 |
| lab12 | 12 | visual-perception | （可选）对比度敏感/Gamma 直觉实验 | 灰度阶梯与可辨阈值示意 |
| lab13 | 13 | more-ray-tracing | 路径追踪：俄罗斯轮盘赌、直接光采样、收敛 | 噪声→收敛多 spp 对比 |
| lab14 | 14 | sampling | 网格/分层/低差异序列；方差对比 | 采样点可视化 + 估计方差图 |
| lab15 | 15 | curves | Bezier / 均匀 B 样条求值与 de Casteljau | 曲线控制点与切线/曲率图 |
| lab16 | 16 | surfaces | Bezier patch、简单细分曲面 | 3D 曲面渲染到 PPM（用 lab 追踪器或软件光栅） |
| lab17 | 17 | light | 点/方向/聚光/面光源模型与衰减 | 多光源场景对比 |
| lab18 | 18 | materials | microfacet GGX 与漫反射对比（理想镜面/玻璃见 lab10） | 材质对比渲染 |
| lab19 | 19 | wave-optics | （可选/低优先）波动光学玩具：衍射图样 | 衍射强度分布图 |
| lab20 | 20 | visibility | 软件深度缓冲、遮挡；简单 CSG 或 occlusion 演示 | z-buffer 与画家算法失败案例对比 |
| lab21 | 21 | rt-hardware | （概念章）软件流水线阶段模拟：顶点→图元→光栅→片元日志 | 每阶段中间结果 dump |
| lab22 | 22 | rasterization | 软件三角形光栅化、边方程、重心插值、z-buffer | 软光栅渲染网格；与追踪结果同场景对比 |
| lab23 | 23 | hardware-features | 软件模拟混合/模板/多边形偏移等特性 | 特性开关对比图 |

**优先级（实现顺序）：**

1. **P0 核心路径**（审核通过后立刻做）：lab00 → lab01 → lab02 → lab03 → lab04 → lab05 → lab06 → lab09 → lab13 → lab22  
2. **P1 扩展**：lab07 → lab08 → lab10 → lab11 → lab14 → lab15 → lab16 → lab17 → lab18 → lab20  
3. **P2 可选**：lab12、lab19、lab21、lab23（概念/硬件特性章，实现价值较低时可在 README 标记 skip 并说明原因）

实现时**按书章顺序推进用户可见的 lab 编号**，但允许先打通 P0 依赖链（例如 ch08 加速依赖 ch03 追踪器骨架）。

### 2.7 每个 Lab 的交付清单

1. `README.md`：对应书章、学习目标、关键公式/伪代码指针、编译运行命令、建议改动的参数、预期现象、常见坑（坐标系、sRGB、弧度制）。
2. `Makefile`：`all` / `run` / `clean` / 可选 `test`。
3. `src/`：可读 C 代码；入口 `main` 解析极简参数（宽高、输出路径、种子）。
4. 运行后在 `labs/chNN-slug/out/` 生成图像。
5. 实现完成后在顶层 `docs/roadmap.md` 勾选状态。

### 2.8 错误与边界行为

- 图像尺寸、路径失败：非零退出码 + stderr 信息。
- 光线求交：拒绝 NaN 传播；ε 偏移避免自交（shadow acne）在追踪系列 lab 统一策略（`t_min = 1e-4` 类似物写进 common 注释或 lab03 并复用）。
- 除零、normalize 零向量：断言或安全返回。
- 不实现通用场景描述语言；场景用 C 代码硬编码（可读性优先）。

### 2.9 测试边界

- 单测：以 lab 内 `--self-test` 为主（向量恒等式、矩阵逆、投影往返、求交已知解）。
- 不引入外部测试框架；断言宏放 `common`。
- 不强制 CI；本地 `make all` + 抽样 `make run` 为验收基线。
- 性能计时（lab08/13）只做相对对比，不设绝对阈值。

### 2.10 文档与进度

- 本文件：设计权威来源。
- `docs/roadmap.md`：用户可读章节对照表与勾选进度（实现阶段生成）。
- 各 lab README：学习笔记式，中文为主，术语可附英文。

## [S3] Out of Scope

- OpenGL/Vulkan/DirectX、GPU 着色器、实时交互窗口。
- 动画系统、物理模拟、角色蒙皮（FoCG 正文不以动画 lab 为主线）。
- 通用场景文件格式（USD/glTF 完整支持）、资产导出到 DCC。
- 大型引擎架构、多线程性能工程（允许简单计时，不写任务系统）。
- 为「补齐硬件章」而实现完整 GPU 驱动级光栅器。
- 自动批改平台、网页前端展示。
- 与书中习题答案的逐题对照（lab 是概念实验，不是习题解答集）。
- **明确跳过（交付审查确认，非静默缩水）**：vendored stb PNG / `cgl_png.h`；`tools/run_lab.*` 一键脚本；lab07 mipmap/LOD；lab15 均匀 B 样条；lab16 细分曲面；lab14 低差异序列（Halton/Sobol）；lab22 与 lab03 同场景 RT 对照图（README 说明实现路径差异即可）。

## Tasks

- [x] T1: 建立仓库骨架与工具链约定 — acceptance: 顶层 Makefile + `common` 可被 lab00 链接；`make -C labs/lab00-toolchain-smoke run` 在 UCRT64 下生成非空 PPM (covers: S2.2, S2.3)
- [x] T2: 实现 common 基础库（vec/color/image/rng）— acceptance: header API 与 S2.5 一致；lab00 使用其中至少 image 写 PPM (covers: S2.4, S2.5)
- [x] T3: 实现 P0 追踪链 lab01–lab06（含 lab03 最小 RT）— acceptance: 各 lab `make run` 产出 README 所述图像；lab04 `--self-test` 全过 (covers: S2.6)
- [x] T4: 实现 lab09 着色与 lab13 路径追踪 — acceptance: 材质球与多 spp 收敛对比图可生成；固定种子可复现 (covers: S2.6)
- [x] T5: 实现 lab22 软件光栅化并与追踪同场景对照 — acceptance: 网格场景 z-buffer 图像正确无缺面；README 说明与 RT 差异 (covers: S2.6)
- [x] T6: 实现 P1 扩展 labs（07/08/10/11/14–18/20）— acceptance: 每个 lab 独立 run 出图或出对比数据；roadmap 勾选 (covers: S2.6, S2.7)
- [x] T7: P2 可选 lab 或明确 skip 标记 — acceptance: lab12/19/21/23 各自有实现或 roadmap 标注 skip 与原因 (covers: S2.6)
- [x] T8: 完善 README/roadmap 与一键脚本 — acceptance: 新人按顶层 README 能在 UCRT64 跑通 lab00 与 lab03 (covers: S2.7, S2.10)
- [x] T9: 全量构建与抽样视觉/自检验证 — acceptance: `make all` 通过；至少 lab03/13/22 输出人工或哈希核对记录在 Report (covers: S2.9)

## Review fixes（2026-09-12 审查收口）

- [x] T10: 顶层 Makefile 改为纯 Make 规则，PowerShell/cmd 可 list/all/test/clean/run/release — acceptance: PowerShell 下 `mingw32-make list` 与 `mingw32-make test` 成功 (covers: S2.3)
- [x] T11: 修正 lab07 README 虚假 mipmap 表述 — acceptance: README 不再声称已实现 mipmap (covers: S2.6)
- [x] T12: `cgl_assert.h` 补 `math.h`；`cgl_mat4_invert` 上提 common；lab05 改用并自检 — acceptance: assert 单独可编译；lab05 `--self-test` PASS (covers: S2.5)
- [x] T13: lab13 `hit_box` 平行轴保护 + `cos_light` 用实际 wi.y — acceptance: `--self-test` PASS；无 NaN 路径 (covers: S2.8)
- [x] T14: 多 lab 写文件失败路径释放图像并 stderr — acceptance: 失败路径含 `cgl_image_free` (covers: S2.8)
- [x] T15: 杂项：lab02 死代码、lab18 文案、lab.mk 注释、lab05/06 写结果 — acceptance: 代码与注释一致 (covers: S2.10)

## Review fixes（2026-09-13 二次审查收口）

独立复审（对照本规格逐条核查 common/构建/24 个 lab）后的修复：

- [x] T16: `make/lab.mk` release 目标特定 CFLAGS 补回 `-I$(COMMON_DIR)`（原写法整体覆盖全局 CFLAGS，`make release` 必然编译失败） — acceptance: dry-run 编译行含 -I；顶层 `mingw32-make release` 实跑成功 (covers: S2.3)
- [x] T17: `make/lab.mk` recipe 语法按 `MSYSTEM`（MSYS2/Git Bash 才设置）选择 POSIX/cmd 分支 — acceptance: PowerShell（无 MSYSTEM）与 Git Bash 两侧 `clean/run` 均成功 (covers: S2.3)
- [x] T18: `cgl_image.c` 写盘错误传播（fprintf/fwrite/fclose 检查）+ `cgl_to_u8` NaN→0；`cgl_div3` 零除断言 — acceptance: 全量 `make test` PASS (covers: S2.4, S2.8)
- [x] T19: lab05/06 写失败退出码传播 + create 失败 stderr；lab05 实际出图并修正 README 虚称（软阴影/一组球体）；lab06 删死变量并重跑 — acceptance: 两 lab `make run` exit 0 且 out/ 有新鲜 PPM (covers: S2.7, S2.8)
- [x] T20: lab08 计时与一致性比对分离（原 aabb 计时含整遍暴力求交） — acceptance: 纯计时数字合理、mismatch=0 (covers: S2.9)
- [x] T21: lab17 补面光源（矩形光采样软阴影 + 硬阴影对比）；lab23 补多边形偏移（共面 decal 深度 bias 开关对比） — acceptance: 自检新增断言 PASS；输出图验证 (covers: S2.6)
- [x] T22: lab14 改为估计量方差（B 批次批均值），新增方差条形图；lab15 补解析导数与曲率着色 — acceptance: 自检 PASS（grid 方差=0、四分之一圆 |κ|≈1） (covers: S2.6)
- [x] T23: lab09/11 README 增「范围与简化」节；lab16/21 README 措辞与实现对齐 — acceptance: 无未声明缩水 (covers: S2.7, S3)
- [x] T24: 清理 `-p` 杂散目录与 lab03 调试残留；lab07 死赋值/泄漏路径；lab20 OOM 部分释放 — acceptance: 仓库无杂散文件 (covers: S2.8)

已知保留项（非缺陷）：lab08 均匀网格 DDA 留作练习（README 已披露）；lab13 NEE 无 MIS 双计（README 已披露）；cgl_assert.h 经 T18 由 `cgl_div3` 引用后不再是死代码。

## 审核请求

请重点确认：

1. **章号/主题映射**是否与你手中的第 5 版一致（尤其是否保留 23 章结构；若你用的是其他分章，直接改 slug 表即可）。
2. **P0/P1/P2 优先级**是否符合你的学习路径。
3. **坐标系与列主序矩阵**约定是否接受。
4. **纯 PPM 默认、PNG 可选**是否够用。
5. **lab12/19/21/23** 是否允许 skip 或降级为概念说明。

审核通过后：将本文件 `status: in-progress`，按 T1→T2→… 在 `feat/fo-cg-labs-curriculum` 分支实现。

> 交付后注：目录命名为 `labs/labNN-slug`（设计草稿中的 `chNN-slug` 未采用，与顶层 Makefile/roadmap 一致）。
> 另注：上图中 `cgl_png.h`、`vendor/`、`tools/` 按 [S3] 明确跳过，实际未创建；`cgl_assert.h` 与 `cgl_selftest.h` 为实现期合理补充。
