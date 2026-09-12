# Lab 4 —— 语法分析（下）：LR(0) 项目集与 SLR(1) 自底向上分析

> 对照龙书 4.5 节（自底向上分析/移进归约）、4.6 节（LR 项与简单 LR 构造）、
> 4.7 节（更强大的 LR，本 lab 只读不实现）、4.8 节（二义文法：悬空 else）
> 源码：[`src/grammar.c`](src/grammar.c)（左递归文法 + FOLLOW）· [`src/slr.c`](src/slr.c)（项目集族 + 表 + 驱动器）

## 这个 lab 解决什么问题

lab3 的 LL 系**从开始符号向下长**，要求文法先消除左递归；
LR 系反过来**从记号流向上归约**（找"句柄"缩回去），**直接吃左递归**——
而左递归恰好是表达"左结合运算"最自然的写法。本 lab 实现 SLR(1)：

```
左递归文法（原始形态，无需变换）
   → 增广 + closure/goto 构造 LR(0) 项目集规范族（一个 DFA）
   → 用 FOLLOW 剪枝填 ACTION/GOTO 表
   → 移进-归约驱动器（显式状态栈）
```

外加一场好戏：**悬空 else 的 shift/reduce 冲突**——自动检测、
按 yacc 惯例"移进优先"消解，并从 trace 里**亲眼验证** else 绑定了最近的 if。

## 快速上手

```bash
cd lab4-parser-lr
mingw32-make
./minicc4.exe tests/cases/expr.mc            # lab3 的表达式集：原样接受
./minicc4.exe tests/cases/ifelse.mc          # if/else、块、空语句
./minicc4.exe -trace tests/cases/ifelse.mc   # 看移进/归约全过程（重点！）
./minicc4.exe -states                        # 规范 LR(0) 项目集族（52 个状态）
./minicc4.exe -table                         # ACTION/GOTO 表
./minicc4.exe -grammar                       # 文法与 FOLLOW 集
bash tests/run_tests.sh
```

## 与 lab3 的第一手对照（同一条表达式 `print 1+2*3;`）

| | lab3 LL(1) | lab4 SLR(1) |
|---|---|---|
| 文法 | 消除左递归 + ε 产生式层（add' term'…） | **原始左递归**（add → add + term） |
| 预测力 | 向前看 1 个符号查 M 表 | 状态栈顶 + 向前看 1 个查 ACTION |
| 推导方向 | 最左推导（自顶向下展开） | 最右推导的逆（自底向上归约） |
| 归约顺序 | 先知道 `1` 是 factor 才知道它是 term… | 先把 `2*3` 归约成 term，再 `1+term` |
| 冲突处理 | 表冲突 = 文法不合格 | 冲突可检测、可按规则消解（悬空 else） |

在 `-trace` 输出里找 `reduce P23/P19/P16/P11`（factor→…→expr）——
**归约顺序恰好是优先级从高到低**，与 LL 从 expr 一路下潜的方向互为镜像。

## 理论精要

### 1. 移进-归约：自底向上的直觉（4.5 节）

栈 + 两个动作：

- **移进**：把输入记号压栈；
- **归约**：栈顶若干符号恰好是某产生式右部（**句柄**），弹出并压入左部。

`1 + 2 * 3;` 的归约链（正是 trace 里的顺序）：

```
INT → factor → term → [1+...]等等 → add → rel → expr
   2*3 先结合: term*factor → term   （乘法优先级的由来！）
```

难点在"何时归约"——这就是 LR(0) 自动机的工作。

### 2. LR(0) 项目与规范项目集族（4.6.1）

**项目** = 产生式 + 圆点：`add → add . + term`（"已看到 add，正等 + "）。

- `closure(I)`：圆点右边是非终结符 B，就把 B 的所有产生式（圆点在开头）加进来；
- `goto(I, X)`：圆点恰好跨过 X 前进一格；
- 从 `[S'→·stmt_list]` 出发 BFS 所有 goto，得到 **52 个状态**（`-states` 查看）。

这个 DFA 识别的是**活前缀**（viable prefix）：句柄左边的部分。
**驱动器的状态栈 = 在这个 DFA 里走过的路径**——LR 分析器 = DFA + 栈，妙极。

### 3. SLR(1) 表构造（4.6.2）

三类填表规则：

```
[A→α·aβ] 且 goto(I,a)=J  =>  ACTION[I,a] = shift J
[A→α·] 且 a ∈ FOLLOW(A)  =>  ACTION[I,a] = reduce A→α     ← SLR 的剪枝
[S'→S·]                  =>  ACTION[I,EOF] = accept
```

"完成项不无限归约、只在 FOLLOW(A) 上归约"就是 SLR 对朴素 LR(0) 的全部改进。
更强的变体（4.7 节，只读不实现）：**规范 LR(1)** 给每项带上精确的向前看
集合（表巨大）；**LALR(1)** 合并"同心"的 LR(1) 状态——**yacc/bison 实现的
正是 LALR(1)**，表大小与 SLR 相当、能力更强。

### 4. 悬空 else：二义文法的经典冲突（4.8 节）

文法故意同时保留：

```
stmt → if ( expr ) stmt           P5
stmt → if ( expr ) stmt else stmt P6
```

状态 49 里同时有完成项 `[stmt → if (expr) stmt ·]` 和
`[stmt → if (expr) stmt · else stmt]`。遇到 `else`：

- **移进**：else 归内层 if（C 语义）；
- **归约** P5：else 归外层 if。

而 `else ∈ FOLLOW(stmt)`（P6 里 stmt 后面跟 else），SLR 剪不掉 →
**shift/reduce 冲突**。yacc/bison 的默认规则：**有冲突时优先移进**——
恰好就是 C 的语义。本 lab 完整复现：构造表时打印冲突报告，
`-trace` 里第 29 步可见 `state 49 <-- else | shift 50`。

> 二义文法 + 冲突消解（优先级/结合性声明，即 yacc 的 %prec %left %right）
> 常常比改写无二义文法得到**更小、更快**的分析表——这是 4.8 节的实践智慧，
> 也是为什么生产级文法几乎都带二义性声明。

### 5. 错误处理

本 lab 的驱动器报错即停（龙书 4.8/练习讨论的错误恢复：弹栈找可恢复状态、
yacc 的 `error` 记号插入——思路与 lab3 的 panic 一致，作为练习）。

## 练习

1. **实现 yacc 式优先级消歧**：删掉文法里分层非终结符（rel/add/term 全并成
   expr），用运算符优先级表消解产生的冲突——体会 4.8 节"二义文法更小更快"。
2. **故意制造 reduce/reduce 冲突**（如 `A→aa|bb` 与 `B→aa`），
   观察构造器直接拒绝。
3. 把 `slr.c` 的 goto 状态查找从线性换成本 lab2 的"子集查重"思路对比——
   两者本质相同（都是"集合→编号"的 memoization）。
4. 用 `-trace` 数一数 `print 1+2*3;` 用了几步归约；对照 lab3 的 `-trace`
   展开步数，理解"LL 先下潜、LR 先归约"。
5. （进阶）给 SLR 加 LALR：把 LR(1) 同心状态合并，表应仍无冲突。

## 龙书 vs 现代

- **LALR/yacc 统治了 30 年后衰落**：bison 仍在维护（GNU 工具链内部使用），
  但 **GCC 2004 年起弃用 yacc 改手写递归下降**，Clang/rustc/TypeScript/Go
  从未用过 LR 生成器。原因：错误恢复质量（Clang 的恢复能"跳到下一个 ;"后
  继给出有用补全建议）、IDE 增量解析、可维护性。
- **LR 思想没有死**：**GLR**（并行维护所有移进/归约分支）用于真正的二义
  文法——C++ 的声明/表达式歧义、自然语言；代表工具 **tree-sitter**
  （GitHub/VS Code/Neovim 的语法高亮引擎）就是增量 GLR。
  理解 GLR 的前提是先懂本 lab 的移进-归约。
- **手写递归下降里悬空 else 根本不是问题**：你在 parse_stmt 里自然地
  `if (match(else)) parse_stmt()`——else 天然绑最近的 if。对照本 lab
  动用一整套机制才"正确地"消解同一个歧义，体会两条路线的工程差异。
- 龙书 4.7 节 LALR 的"同心合并"算法，在 bison 里至今仍是核心；但新项目
  选择分析器时的默认答案已经是"手写 RD，或 ANTLR4，或 tree-sitter"。
