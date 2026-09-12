# Lab 11 —— 依赖分析与局部性：距离向量、合法性驱动的变换与缓存实验

> 对照龙书第 11 章（数据依赖与依赖分析）与第 5 章 5.7 节（数组下标的
> 仿射形式）；语言扩展二维数组 `int m[N][M]`（gen 层行主序降糖，
> 四个执行引擎零改动）
> 源码：[`src/opt.c`](src/opt.c)（新增 Pass F：仿射识别/距离向量/门禁展开/循环交换
> 与 `-deps`/`-space` 观测点）· [`src/gen.c`](src/gen.c)（行主序降糖）·
> [`tests/cases/dep.mc`](tests/cases/dep.mc)（依赖演示）· [`tests/cases/space.mc`](tests/cases/space.mc)（迭代空间）

## 这个 lab 解决什么问题

lab10 教会你"找结构"（支配树、自然循环），但动循环之前还差一个根本的
问题：**怎么知道改写是安全的？** 第 11 章给出答案——依赖分析。本 lab
把它落到 MiniC 上，并顺手补上依赖分析最自然的舞台：

- **二维数组** `int m[N][M]`：symtab 记两维；gen 层做**行主序扁平化**
  `m[i][j]` → `t=i*M; t2=t+j; v=m[t2]`——纯既有四元式，四个执行引擎
  零改动（描述符按总元素数 N*M 登记）；eval 直接走 AST，在树上镜像
  同一套行主序寻址。
- **一维依赖分析**：同一循环内两条访存语句逐维解距离向量 δ=cA−cB，
  分类 true（写→读）/anti（读→写）/output（写→写）/none；
  `-deps` 打印清单。
- **合法性驱动的变换**：lab10 的展开升级为「依赖检查通过才展开」，
  并因此**解禁了数组写体**（无携带依赖的初始化循环现在能展开）；
  完美二重嵌套做**循环交换**——while 降糖布局对称到只需互换两条 IF。

```
-O 管线（lab10 框架内新增 Pass F）：
四元式 → 折叠/传播/CSE →【交换 → 外提 → 门禁展开】→ DCE → 窥孔
            ↑ 交换独占一轮扫描：LICM 的 preheader / 展开的复制体
              都会破坏七块规范布局，混排会让嵌套永远轮不到交换
观测点（固定作用于 gen 后、-O 前的原始 IR，输出走 stderr）：
-deps 距离向量清单   -space 迭代空间网格 + 依赖箭头
```

## 快速上手

```bash
cd lab11-locality
mingw32-make
./minicc11.exe -deps  tests/cases/dep.mc     # 每个环的访存对 + 距离向量 + 类别
./minicc11.exe -space tests/cases/space.mc   # 迭代空间网格 + 对角依赖箭头
./minicc11.exe    -ir tests/cases/swap.mc > before.txt
./minicc11.exe -O -ir tests/cases/swap.mc > after.txt   # 外层已改测 j：交换生效
bash tests/run_tests.sh                      # 四十三项全绿（含 lab10 全量回归）
bash tests/bench.sh                          # 缓存计时：行序vs列序、i-j-k vs i-k-j
```

`-deps` 输出示例（dep.mc 第一环，`a[i] = a[i-1]+1`）——后写的语句喂给
下一迭代的读，是教科书式的真依赖：

```
[deps] loop head=B1 latch=B3 stmts=2
[S2->S1 a dist=1 true]
```

`-space` 输出示例（space.mc 的对角递推）——网格里字母是依赖源实例样例，
箭头行给出位移向量：

```
[space] nest i in [1,6) x j in [0,5)
[space]      j=0   j=1   j=2   j=3   j=4
[space] i=1  .    A    .    .    .
...
[space] arrows:
[space]   A: S2->S1 dist=(1,-1) true at (i=1,j=1)
```

## 与 lab10 的第一手对照

| | lab10 循环优化 | lab11 依赖分析与局部性 |
|---|---|---|
| 安全判据 | 结构守卫（直线体/偶数界/CALL 禁） | 距离向量（合法性可证明） |
| 数组写体 | 一刀切禁止展开 | 无携带依赖即放行（unrollnew.mc 钉住） |
| 变换范围 | 单层循环（外提/展开） | 二重嵌套（循环交换） |
| 新表示 | CFG / 支配集 | 仿射下标 / 距离向量 |
| 观测点 | -cfg（控制流形状） | -deps/-space（数据依赖形状） |

## 理论精要

### 1. 仿射下标识别（教学版口径）

下标限定「iv」「iv ± 常量」或纯常量。实现上从访存四元式的下标地址沿
定义链回溯（临时单定义是 gen 的不变量，一张 DefMap 就够）；叶子变量须
是"区域不变符号"或"规范计数器"（至多一次常量 init + 恰一次
`x = x ± c` 自增）。二维数组先逆向解开降糖链：`z = t1 + 列` 且
`t1 = 行 * 常量` → 恢复出（行, 列）两个仿射式。解不出来记 unknown——
**分析可以弱，结论必须稳**：unknown 一律阻断后续变换。

### 2. 距离向量与依赖分类（11.1~11.2 节）

设 B 在迭代 I+δ 访问的元素与 A 在迭代 I 相同，逐维解
`iv + cA = iv + δ + cB` 得 δ = cA − cB。原始方程解出负方向时把角色翻转
再解一次——后写前读的配对里往往藏着经典真依赖（写喂下一迭代的读），
`dep_between` 统一报告**时间上可实现**的方向。读写方向定类：写→读
true、读→写 anti、写→写 output、读→读 none。

### 3. 合法性驱动的展开门禁（两章呼应）

展开 ×2 复制体后实例相对顺序不变，顺序语义天然保持；但按本章口径取保守
规则——体内存在任何跨迭代记忆依赖（δ 任一分量非零）或 unknown 访存即
放弃。收益是双向的：`a[i]=a[i-1]+1` 这类流依赖循环被挡在门外，而
`a[i]=i*2` 这类无依赖的**数组写体**获得 lab10 不可能给的展开资格。

### 4. 循环交换：布局对称性的红利（11.5 节）

完美嵌套的合法性 = 所有距离向量已知且交换后不出现逆序分量：
原序保证 δ 字典序非负，交换后迭代键变 (j,i)，非法 ⇔
`d2<0 || (d2==0 && d1<0)`——反三角流（如转置型递推 m[i][j]=m[i-1][j+1]）
被精确拒绝。在依赖分析之外还有三道**结构性门禁**，各自堵一类依赖
分析看不见的风险：①体区必须直线（CALL/PARAM 的实例顺序会随交换
改变、被调方隐藏访存不可见）；②非访存四元式里的用户标量只允许两个
归纳变量——浮点归约 `acc = acc + m[i][j]` 携带的跨迭代流依赖交换后
累加次序改变（swapacc.mc/swapcall.mc 钉住）。实现吃足了 while 降糖的
对称布局：内外两条 IF 互换条件、两个步进对互换计数器、转发块的
init 换成新内层的 `i=0`、外层头前插一次性的 `j=0`——一条指令都不用
挪。限制也来自这里：只接受两维 init 都给出常量 0 起步的矩形嵌套
（非零起点直接放弃；空 init 起点未知同样放弃，swapinit.mc 钉住），
体内有 print 即拒绝（实例顺序变 =
输出顺序变，铁律不容）。管线位置上交换**独占一轮扫描**先行：实测 LICM
先插 preheader 会破坏七块布局，让外层嵌套永远轮不到交换。

### 5. 局部性为什么重要（bench 实验）

行主序布局下列序遍历每次跳跃 M×8 字节；i-j-k 矩阵乘让 b[k][j] 的内层随
k 整行整行地跳。`tests/bench.sh` 用原生代码实测两组对比（软验证）：列序
约慢 1.5×，ikj 约快 1.5×——交换正是让"热数据在同一轮里挨在一起"的
编译期手段。

## 代码导读（建议按此顺序读）

| 文件 | 内容 | 对应龙书 |
|---|---|---|
| [`src/parser.c`](src/parser.c) | parse_decl/parse_one_subscript：双维声明与下标个数核对 | — |
| [`src/gen.c`](src/gen.c) | A_INDEX/A_STORE 行主序降糖；描述符登记 N*M 总元素数 | 5.7、6.4 |
| [`src/eval.c`](src/eval.c) | 树上的行主序镜像（flat = i*M + j） | — |
| [`src/opt.c`](src/opt.c) | Pass F：DefMap/RegDefs → resolve_affine → memref_of → dep_between → swap_nest/nest_find；deps_dump/space_dump | 第 11 章 |
| [`tests/cases/unrollnew.mc`](tests/cases/unrollnew.mc) / [unrolldenied.mc](tests/cases/unrolldenied.mc) | 门禁正反例（双 dump 冻结） | — |
| [`tests/cases/swapbad.mc`](tests/cases/swapbad.mc) | 反三角流拒绝交换 | — |

## 练习

1. **GCD 测试**：把"系数必须相同"放宽到整数方程有整数解的判定
   （GCD 测试），让 m[i][j] vs m[j][i] 从 unknown 变成可回答。
2. **Banerjee 不等式**：把循环界纳入约束，输出方向向量代替精确距离，
   覆盖非常量界与非规范起点的情况。
3. **分块 tiling**：把 bench [3] 的 ikj 升级成 B×B 分块版本，对比耗时；
   思考为什么 MiniC 现有语法写 tiling 要复制边界循环。
4. **符号界可视化**：`-space` 目前只认常量界；让网格支持符号界需要
   符号约束求解——设计一个最小方案。
5. **向前替换**：解三角递推 y[i] -= L[i][k]*y[k] 型依赖，说明为什么
   标量重命名（scalar expansion）是其前置条件。

## 龙书 vs 现代

- **依赖分析的生产形态**：Polyhedral 编译（LLVM Polly、Pluto）把整段
  程序表示成整线性约束系统，调度变换是一组 ILP 解；本章的距离向量是其
  最小特例。
- **交换的守门人**：LLVM 的 LoopInterchange 在依赖矩阵之外还要看分支
  概率与缓存代价——合法性与收益分开建模是工业共识。
- **SIMD 与依赖**：自动向量化（LoopVectorize）的 first-order-recurrence
  检查就是本章 true-dependence 分析的位宽版。
- **缓存的现代语境**：硬件预取器能吞掉固定小步长，bench [2] 的差距要
  在大工作集下才显形——这也是生产级 tiling 参数靠性能模型而非固定阈值
  的原因。

---

*上一篇：[lab10-loop](../lab10-loop/README.md) · 总目录：[README](../README.md) · 配套勘误：[docs/MODERN-NOTES.md](../docs/MODERN-NOTES.md)*
