# Lab 10 —— 循环分析与优化：支配树、自然循环、LICM 与循环展开

> 对照龙书第 10 章（指令级并行：依赖关系、展开为调度创造窗口）与 9.6 节
> （自然循环）；语言扩展 `for` 循环（parser 拼装降糖，引擎零改动）
> 源码：[`src/opt.c`](src/opt.c)（新增 Pass E：CFG/支配树/自然循环/LICM/展开）·
> [`src/parser.c`](src/parser.c)（for 降糖）· [`tests/cases/lics.mc`](tests/cases/lics.mc)（主演示）

## 这个 lab 解决什么问题

lab9 的优化都发生在基本块内；循环——程序里最热的代码——还 untouched。
本 lab 把分析半径从块扩展到**整个循环**：先在 CFG 上找出"谁支配谁"、
把回边圈成自然循环，然后做两件最经典的循环优化：

- **LICM 不变量外提**：操作数在环内不被重定义的纯运算，移出循环只算一次；
- **计数循环展开 ×2**：`for (int i = 0; i < 8; i = i + 1)` 复制体，
  减半循环开销、扩大基本块——为第 10 章的指令级并行创造窗口。

同时给 MiniC 加上 `for (init; cond; step)`——它是**纯语法糖**：
parser 直接拼装成既有 AST 组合 `{ init; while (cond) { body; step; } }`，
gen 与四个执行引擎零感知。

```
-O 管线（lab9 框架内新增 Pass E，外层不动点照旧）：
四元式 → 折叠/传播/CSE →【支配树 → 自然循环 → LICM → 展开】→ DCE → 窥孔
```

## 快速上手

```bash
cd lab10-loop
mingw32-make
./minicc10.exe    -ir tests/cases/lics.mc   > before.txt   # 循环内的 g*n
./minicc10.exe -O -ir tests/cases/lics.mc   > after.txt    # 外提到 preheader
diff before.txt after.txt                                  # 外提效果肉眼可见
./minicc10.exe -cfg tests/cases/lics.mc                    # 块/边/支配集/自然循环
bash tests/run_tests.sh                                    # 十一套 -O 原生对拍全绿
bash tests/bench.sh                                        # 计时软验证：外提/展开前后的耗时对比
```

`-cfg` 输出示例（lics.mc，优化前）——回边 B3→B1 圈出自然循环 {B1,B3}：

```
B1: [4..5] succ={B2,B3} dom={B0,B1}
B3: [7..13] succ={B1} dom={B0,B1,B3}
loop: head=B1 latch=B3 blocks={B1,B3}
```

## 与 lab9 的第一手对照

| | lab9 局部优化 | lab10 循环优化 |
|---|---|---|
| 分析半径 | 单个基本块 | 支配树 + 自然循环（跨块） |
| 找热点 | 无（块平等） | 回边识别 → 自然循环 |
| 不变量 | 常量表即真理 | 操作数在环内无重定义 ⇒ 外提进 preheader |
| 数据流方程 | 反向活跃变量（DCE） | 同一套范式再解一遍支配关系（前向 ∩） |
| 安全网 | CALL 清缓存 / 零除数不折叠 | 环内有 CALL 整环放弃外提；体含数组写不展开 |

## 理论精要

### 1. 支配关系与自然循环（9.6 节）

支配 = 从入口出发的每条路径都经过它。迭代方程 `dom[b] = {b} ∪ ∩ dom[pred]`
沿 CFG 前向传播至不动点——和活跃变量分析同一套范式，方向相反、交并互换。
有了支配关系，**回边**定义为 b→h 且 h 支配 b；从回边出发向后收集（遇头
为止）得到**自然循环**。可识别的循环才可优化——这是全部循环优化的地基。

### 2. preheader：给循环开一扇前门

外提的代码需要一个"进入循环前恰好执行一次"的安放点。做法：在 header 的
LABEL 之前插入 `[LABEL Lpre; 外提码; GOTO Lh]`，并把函数里所有指向循环头
的跳转按所在块分流——环外的改指 Lpre，环内的闩边保持 Lh。一个实现细节
曾让我们翻车：**必须先摘除候选、再插 preheader**——反过来做会让候选的旧
下标整体偏移，删错指令（对拍当场抓住）。

### 3. LICM 的保守性清单

外提条件：纯运算；不含除/模（除零崩溃不允许被提前或消失）；两个操作数
在环内无任何重定义；**环内出现 CALL 即整环放弃**——被调方可能改任何全局，
精确判定需要 mod/ref 分析（lab12 的内容）。结果临时是系统临时（单一定义、
用户不可见），因此移动定义位置永远安全。

### 4. 循环展开与依赖（第 10 章入口）

展开 ×2 的规范形态：单闩尾部 GOTO 回头、头部恰为 `if i < N goto 体`
（N 为正偶常量）、步进恰为 `i = i + 1`、**直线体**（无标号/跳转/CALL/
数组写）、体内不改归纳变量，且**归纳变量的入口初值已知并使趟数
N − 初值为偶数**——只看上界 N 的奇偶不够：`for (i = 1; i < 8; …)` 的
上界是奇数本就被拒，而 `i = 1, N = 8` 这种"偶上界奇趟数"若盲目展开，
最后一对的第二个体会以 `i == 8` 多执行一次。初值未知同样放弃
（unrollinit.mc 钉住这两个方向）。收益：减半
比较与跳转、扩大基本块给 CPU 调度空间；风险：若体存在真依赖（如
`a[i] = a[i-1]+1`），盲目展开会算错——合法性判定正是 lab11 的主题。
实现上还有一条容易踩的坑：克隆时只给**区域内定义**的临时换新名——
LICM 刚外提的不变量在体内只剩对它的"使用"，若把该使用也改名，克隆体
将引用一个从未定义的临时、读出垃圾值（`licsunroll.mc` 专门钉住这条）。

## 代码导读（建议按此顺序读）

| 文件 | 内容 | 对应龙书 |
|---|---|---|
| [`src/opt.c`](src/opt.c) | Pass E：cfg_build/dominators/find_loops/licm_loop/unroll_loop/-cfg dump（主文件尾段） | 9.6、第 10 章 |
| [`src/parser.c`](src/parser.c) | parse_for：拼装降糖 + continue 限制说明 + eat_semi 重构 | — |
| [`src/token.h`](src/token.h)/lexer.c | T_KW_FOR 关键字 | — |
| [`tests/cases/lics.mc`](tests/cases/lics.mc) | 外提演示（双 dump 冻结对拍） | — |
| [`tests/cases/licsunroll.mc`](tests/cases/licsunroll.mc) | LICM×展开组合回归（克隆必须共享外提临时原名） | — |

## 练习

1. **余数循环**：去掉"N 可整除"限制——展开 ×2 后补一个跑 ⌈余项⌉ 次的
   尾循环（需要克隆条件与出口重定向）。
2. **continue 支持**：把 step 深拷贝到 for 体内每个 continue 点
   （AST 克隆器），解除当前的语言限制。
3. **list scheduling 预研**：解释为什么先要有寄存器驻留（lab8 练习 1）
   调度才有意义；给出你设计的延迟模型表。
4. **强度削减**：循环内 `t = i * 8` 用 `i += 8` 的伴随归纳变量替代——
   需要识别基本归纳变量与派生归纳变量的家族关系。
5. **支配树的快速算法**：把迭代法换成 Cooper-Harvey-Kennedy 或
   Lengauer-Tarjan，测 func.mc 上的比较次数差异。

## 龙书 vs 现代

- **支配树无处不在**：从 LICM 到 SSA 构造到异常处理表，支配/后支配是
  中端的通用坐标系；现代用半 NCA/LT 算法近线性求解。
- **展开的动机变迁**：从填 VLIW 槽位到喂乱序核心再到"给向量化铺路"；
  LLVM 按成本模型决定展开倍数，甚至选择不展开。
- **循环优化的工业形态**：LoopPass 管线（LICM→展开→交换→分块）顺序
  即调参；正确性同样靠依赖分析守门——与 lab11 的分工在这里预告。
- **语义保真的验收学**：本章"先摘候选再插入"的下标陷阱，工业界以
  Alive2 式变换验证器自动捕获——思路同源：让机器替你盯住不变量。

---

*上一篇：[lab9-opt](../lab9-opt/README.md) · 总目录：[README](../README.md) · 配套勘误：[docs/MODERN-NOTES.md](../docs/MODERN-NOTES.md)*
