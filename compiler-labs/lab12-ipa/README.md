# Lab 12 —— 过程间分析：调用图、内联、过程间常量传播与 mod/ref

> 对照龙书第 12 章（过程间分析）；**语言零扩展**——MiniC 冻结在 lab11
> 状态，全部实现在 IR 层，前端/gen 层与四个执行引擎零改动
> 源码：[`src/opt.c`](src/opt.c)（新增 Pass G：调用图/`-cg` 观测点/IPCP/
> 内联/mod-ref 集合与四处放宽点）· [`tests/cases/inline.mc`](tests/cases/inline.mc)（内联拼接）·
> [`tests/cases/ipconst.mc`](tests/cases/ipconst.mc)（must 交集）· [`tests/cases/pure.mc`](tests/cases/pure.mc)（放宽证据）

## 这个 lab 解决什么问题

前 11 个 lab 的全部优化都只盯着**一个函数段**看。跨过函数边界会发生什么？
`probe(i)` 每轮迭代都在调，它会不会改掉我的全局变量？`f(7)` 传的是常量，
函数体里的算术能不能提前折好？答案都属于第 12 章。本 lab 落地四件事：

- **调用图 + `-cg`**：结点 = 函数段，边 = CALL 四元式。MiniC 无函数指针、
  函数先定义后用——图是静态精确的（现代编译器要先做去虚化才拿得到的
  东西，教学语言免费送）；DFS 三色标记递归环。
- **函数内联**：叶子优先、体积阈值。机制 = callee 四元式拷进 caller 的
  CALL 位置：PARAM 改写成 ASSIGN 绑定、RETURN 改写成对结果临时的 ASSIGN、
  临时重编号、标号按次重映射。
- **IPCP 简化版**：逐形参收集**全部调用点实参的常量 must 交集**，非 ⊥
  则代入函数体重跑折叠；与调用图迭代至不动点——常量能顺着中间函数
  一层层渗透下去。
- **mod/ref 入门版**：每函数 may-write 全局名集 + pure 标记，据此把
  lab9 的"CALL 清空全部缓存"、lab10 的"环内 CALL 整环放弃"等四处一刀切
  **精确化为"杂质 CALL 才保守"**——三章在此收口呼应。

```
-O 管线（lab11 框架内新增前置段）：
四元式 →【一轮局部折叠 → CG/IPCP 不动点 → 内联】→ 建 mod/ref 集合
        → 折叠/传播/CSE → 循环级 → DCE → 窥孔   ← 各 pass 的 CALL 门禁
                                                  现在按纯度分档
观测点（固定作用于 gen 后、-O 前的原始 IR）：
-cg 调用图转储（stdout，对齐 -cfg 惯例）
```

## 快速上手

```bash
cd lab12-ipa
mingw32-make
./minicc12.exe -cg tests/cases/cg.mc          # 调用图 + 叶子/递归标注
./minicc12.exe -O -ir tests/cases/pure.mc     # 放宽后的循环形态（见下）
bash tests/run_tests.sh                       # 六十五项全绿（含 lab10/11 全量回归）
bash tests/bench.sh                           # 缓存计时软验证（继承 lab11 三组实验）
```

`-cg` 输出示例（cg.mc）——自递归一眼可见：

```
== callgraph ==
main -> mid, fact
leaf  [叶子]
mid -> leaf
fact -> fact  [递归]
```

内联前后对照（inline.mc 主段节选）——嵌套实参 `addk(base, twice(6))`
先内联里层再喂外层；形参绑定在各 PARAM 原位就地落位、形参逐实例铸
成系统临时，随后被常量传播+折叠整体消解：

```
$ ./minicc12.exe -O -ir tests/cases/inline.mc
main():
  base = 10
  print 17       ← addk(3,4) 全部折叠成常量
  print 32
  print 112      ← 嵌套实参链先里后外依次落位折叠
  print 20
  return

addk(a, b):
  t1 = a + b     ← 未被调用的 callee 原样保留
  t2 = t1 + base
  return t2
```

pure.mc 的统计摘要——三个数字各对应一处放宽（死纯调用删除 / 绕纯调用
外提 / 含纯调用的体展开），而 `call probe` 因局部数组门禁保持调用形态：

```
[opt] 常量折叠 0、常量传播 0、CSE 0、死代码 1、窥孔 3、外提 1、展开 1、交换 0、内联 1、IPCP 形参 0
```

## 与 lab11 的第一手对照

| | lab11 依赖分析与局部性 | lab12 过程间分析 |
|---|---|---|
| 分析范围 | 单个函数段内的循环 | 跨函数段的调用关系 |
| 安全判据 | 距离向量（记忆依赖可证明） | mod/ref 集合（写集可证明） |
| 变换对象 | 循环结构（外提/展开/交换） | 调用本身（拼接/代入） |
| 新表示 | 仿射下标 / 距离向量 | 调用图 / may-write 集 |
| 对旧门禁 | 展开的 CALL 一刀切保留 | 四处 CALL 门禁按纯度分档 |
| 观测点 | -deps/-space（stderr） | -cg（stdout，对齐 -cfg） |

## 理论精要

### 1. PARAM↔CALL 配对：为什么必须回扫

实参求值会穿插：编译 `f(1, g(2))` 得到 `param 1; param 2; call g;
(t0); param t0; call f` ——外层的 `param 1` 排在内层 `call g` **之前**
（每个实参求值完立刻发自己的 param）。所以"CALL 前面的 argc 条 param"
这个直觉是错的。正解是从 CALL 反向扫描配深度计数：遇 CALL 深度 +1，
遇 PARAM 时深度为 0 才属于本调用、否则抵消一层。DCE 删调用、IPCP 收
交集、内联改绑定，三处共用这一个 `bind_params`。

### 2. 内联的拼接机制与两个不变量

拷贝体要闯过三关：(a) 形参绑定 = 把每条 PARAM **原位**改写为
`ASSIGN 形参新名 = 实参文本`——原位是语义关键：PARAM 的执行时刻就是
实参值的捕获时刻，实参间穿插嵌套调用时（`f(x, g(1))` 且 g 改写全局
x），挪到 call 点统一重读会拿到"迟到"的值（insline.mc 钉住）；形参
随之逐实例铸成系统临时——绑定拆回各自位置后多个实例不再连续，共享
形参名会互相踩踏；(b) RETURN y 改写成 `ASSIGN 结果临时 = y` 再跳出口
标号——体中 return 不能简单删掉，否则剩余拷贝体会被误执行；gen 补的
隐式尾 RETURN 用返回类型零值填充（int "0"、float "0.0"）；(c) 名册
临时逐一铸新名并**继承静态类型**（类型表缺失会让后端把 float 按 int
错译）、标号整体加偏移。后两者守的是两条地基不变量：**临时单一定义**
（CSE/DCE 的前提，同一 callee 内联两次不能共享 t 名）与**标号全程序
唯一**。局部数组的 callee
被门禁拒绝：数组声明不发清零指令，两个实例共享基名会把上一实例的残值
带进下一实例（pure.mc 的 probe 正是靠这道门禁保持调用形态，让放宽效果
可见）。用户名（形参/局部标量）不需要改名——声明必带初始化，顺序实例
复用同名安全。

### 3. IPCP：must 交集与不动点渗透

对形参 k 的所有调用点取交集：任一实参非常量 → ⊥；全部一致才代入。
代入后重跑局部折叠，下一轮再收交集——**上游函数被代入折叠后，它传给
下游的实参文本就变成了立即数**，常量因此沿调用链逐层渗透。实测：
`outer(7)` → outer 体折叠出 `inner(m*10)` 的实参 `70` → inner 的交集
{70} 成立 → inner 体出现 `i * 70`。三条保守门禁：递归函数的形参一律 ⊥；
体内重定义过的形参整个放弃（替换会越过重定义点取错值）；只代 int
（浮点记法往返易错）。签名与 params[] 不动——VM 按位置绑定的多余槽位
无人读，四个执行引擎因此零改动。

### 4. mod/ref：把"CALL 危险"量化成"CALL 可能写哪些名字"

利用 gen 名字唯一化：顶层声明只在主段定值 ⇒ "主段定值过的非临时名 ∪
is_global 数组基名"恰是全局名全集 G。MW(f) = 直接写 G 中名字 ∪ callees
传递闭包（不动点）；PURE(f) = MW 空 ∧ 无 PRINT ∧ callees 全纯。据此
四处放宽：

| 放宽点 | lab9~11 口径 | lab12 口径 |
|---|---|---|
| CSE 缓存（pass B） | CALL 清空一切 | 只 kill MW(callee) 里的名字 |
| LICM 环门禁 | 环内有 CALL 即放弃 | 杂质 CALL 才放弃 |
| DCE | 从不删 CALL | 死的纯调用连同 param 串整删 |
| 展开体检查 | 体含 CALL 即拒绝 | 仅杂质 CALL 拒绝 |

删纯调用的 param 串时有个陷阱：param 可能散在更早的基本块里（实参求值
带控制流时），留下孤儿 param 会顶进下一个 CALL 的实参栈（VM 按栈顶取
argc 个）——必须就地一并标 DEAD。**诚实声明**：纯函数仍可能死循环或
除零，删掉对它的调用会改变终止性与崩溃时机——这与既有 DCE 删除死掉的
除法是同一类权衡（可观察输出流不变的铁律不含这两者）。

### 5. 为什么交换的 init 门禁不跟着放宽

nest_find 拒绝转发块里的 CALL，理由是**执行时机**（交换会挪动 init 的
求值点），不是读写效果——mod/ref 帮不上忙。这提醒我们：每条保守规则
都要问清它防的到底是什么，别把不同类的风险捆在一起放宽。

## 代码导读（建议按此顺序读）

| 文件 | 内容 | 对应龙书 |
|---|---|---|
| [`src/opt.c`](src/opt.c) | bind_params/func_idx：PARAM 回扫配对与寻段（公共小件在前部） | 12.x |
| [`src/opt.c`](src/opt.c) | modref_build/nset_*：MW 集合与 PURE 标记的不动点 | 12.3 |
| [`src/opt.c`](src/opt.c) | cg_dfs/cg_dump：三色 DFS 与 -cg 转储 | 12.1 |
| [`src/opt.c`](src/opt.c) | ipcp_round/ipcp_apply/subst_formal：must 交集、代入与使用槽位清单 | 12.2 |
| [`src/opt.c`](src/opt.c) | inline_candidate/inline_at/inline_driver：门禁、拼接与驱动 | 12.x |
| [`tests/cases/inline.mc`](tests/cases/inline.mc) | 嵌套实参/同 callee 双实例/void 叶子（双 dump 冻结） | — |
| [`tests/cases/ipconst.mc`](tests/cases/ipconst.mc) | 交集成立 vs 冲突 ⊥ 正反例（+ [opt] 统计快照） | — |
| [`tests/cases/pure.mc`](tests/cases/pure.mc) | 放宽三证据 + 局部数组挡内联（+ 统计快照） | — |

## 练习

1. **非叶内联**：允许内联"只调用了可内联函数"的非叶子，按逆拓扑序
   处理；想清楚为什么现在的叶子优先天然免疫递归。
2. **未引用段清理**：内联清空全部调用点后，callee 段还留在 IR 里——
   加一个段级 DCE（注意：主段不可删；-ir 输出会变化，记得冻结新期望）。
3. **返回值常量传播**：把 IPCP 扩展成"返回常量的函数"，调用点直接替换
   成常量并删调用；与内联的收益重叠在哪？什么时候值得做？
4. **跨块常量实参**：IPCP 只认基本块内折叠出的立即数；设计一个过程间
   稀疏条件常量传播（SCCP）的最小格子模型，说明 must 交集差在哪。
5. **内联成本模型**：INLINE_MAX_BODY 是常数阈值；仿照 LLVM inliner
   把"调用点收益"（常量实参特化、只被调用一次）纳入体积核算。

## 龙书 vs 现代

- **内联的生产形态**：LLVM 的 always-inliner + 成本模型 inliner 是管线中
  权重最高的 pass 之一——"单笔收益最大的优化"；本章的体积阈值是其
  最粗糙特例，练习 5 是通往真实成本模型的门。
- **IPCP 的现代名字**：IPSCCP（interprocedural sparse conditional constant
  propagation）——SSA + 稀疏让格模型更简洁，must 交集迭代的框架未变。
- **mod/ref 的工业版**：别名分析 + MemorySSA。lab9 那条"CALL 清空一切"
  在生产编译器里就是靠它们才敢下手 CSE/LICM/DCE 的——本 lab 把这条
  保守规则的来龙去脉走通了一遍。
- **闭世界假设**：所有过程间分析默认全部代码可见；动态链接/JIT/FFI
  打破它——LTO 是工业界的折中答案。MiniC 的单文件世界没有这个问题，
  这也是教学优势点。

---

*上一篇：[lab11-locality](../lab11-locality/README.md) · 总目录：[README](../README.md) · 配套勘误：[docs/MODERN-NOTES.md](../docs/MODERN-NOTES.md)*
