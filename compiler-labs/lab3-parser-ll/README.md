# Lab 3 —— 语法分析（上）：递归下降与 LL(1) 预测分析

> 对照龙书 4.2 节（上下文无关文法）、4.3 节（适合自顶向下的文法）、4.4 节（自顶向下分析）
> 源码：[`src/rd_parser.c`](src/rd_parser.c)（Part A 递归下降）· [`src/grammar.c`](src/grammar.c) + [`src/ll_parser.c`](src/ll_parser.c)（Part B 表驱动 LL(1)）
> 词法器是 lab1 的精简复刻（[`src/lexer.c`](src/lexer.c)）。

## 这个 lab 解决什么问题

lab1 产出记号流之后，接下来要回答"这串记号是不是一个合法程序"——
并且要**按文法结构**回答（哪些是表达式、谁是谁的操作数）。
本 lab 用**自顶向下**（从开始符号出发，向句子生长）的两种经典姿势分析 MiniC v2：

```
stmt → print expr ; | expr ;
expr → ...（6 层优先级，见下）
```

**两个分析器对同一输入对拍**（默认模式），这是本 lab 的核心体验：

```bash
cd lab3-parser-ll
mingw32-make
./minicc3.exe tests/cases/expr.mc       # 递归下降求值(20个print) + LL(1) 识别
./minicc3.exe -ff                       # 打印 FIRST/FOLLOW 集
./minicc3.exe -table                    # 打印 LL(1) 分析表（对照龙书 4.4.2 手算）
echo "print 1+2*3;" | ./minicc3.exe -ll -trace -    # 看每一步的分析栈！
bash tests/run_tests.sh
```

## Part A：手写递归下降（rd_parser.c，龙书 2.4/4.4.1）

### 形状：每个非终结符一个函数

```c
Val parse_add(void) {                 /* add → term { (+|-) term } */
    Val acc = parse_term();
    while (cur.type == T_PLUS || cur.type == T_MINUS) {
        ...  acc = acc ± parse_term();
    }
    return acc;
}
```

函数调用链 `expr → equality → rel → add → term → factor` 就是优先级：
**调用越深，优先级越高、结合越紧**。`while` 循环处理同优先级的连续运算。

### 左递归：LL 系的原罪（龙书 4.3 节）

我们想要的文法是 `add → add + term | term`（左递归）——它天然表达左结合。
但左递归会让递归下降**无限递归**（`add()` 第一件事还是调 `add()`）。
龙书 4.3 节的标准变换：

```
add → term add'          add' → + term add' | ε     （消除左递归）
```

### 陷阱：消除左递归会破坏结合性！

按 `add'` 的字面右递归结构求值，`10-3-2` 会算成 `10-(3-2)=9`。
解决办法是把文法等价改写成循环形式（龙书 4.3 节同一小节给出）：

```
add → term { (+|-) term }
```

循环里**边扫边累计**（`acc = acc - term`），左结合天然成立。
本 lab 的 rd_parser 全部用循环形式；lab0 运算符栈"等于也弹"完成的是同一件事。
**这是手写 parser 不踩坑的关键细节**，龙书一笔带过，工程上天天遇到。

### 为什么它还负责求值？

递归下降函数的返回值就是龙书第 5 章说的**综合属性**——
`parse_add` 返回子表达式的值，正是"语法制导翻译"的雏形。
lab3 先享受这个红利让程序"能跑"；lab5 再把它正规化成 AST。

## Part B：表驱动 LL(1)（grammar.c + ll_parser.c，龙书 4.4.2~4.4.3）

Part A 的文法"长在函数结构里"；Part B 把文法变成**纯数据**（`PRODS[]` 表），
其余全部自动计算——这就是语法分析器生成器的内核：

1. **FIRST 集**：非终结符能推出的第一个终结符有哪些（`compute_first_follow` 不动点迭代）；
2. **FOLLOW 集**：非终结符后面可能跟什么终结符；
3. **LL(1) 表** `M[A][a]`：栈顶非终结符 A 遇到输入 a 时该用哪条产生式；
   同一格填两次 = 冲突 = 文法不是 LL(1)；
4. **驱动循环**（`ll_parser.c`）：显式栈，栈顶终结符→匹配、
   非终结符→查表展开，直到栈空。

用 `-trace` 看一遍 `print 1+2*3;` 的分析过程（每步：栈 | 剩余输入 | 动作），
**这张逐步展开的表就是最左推导**——LL 分析的理论本质一目了然。

### panic 错误恢复（龙书 4.4.3）

表驱动分析遇到错误时：弹掉出事的非终结符 A，**丢弃输入记号直到遇到
FOLLOW(A) 里的"同步记号"**，在那里重新续上。效果见 `tests/cases/errors.mc`：
第 3、4 行两处错误一次报完，第 5 行照常分析。

## 文法对照（本 lab 的完整 MiniC v2 文法）

```
stmt_list → stmt stmt_list | ε
stmt      → print expr ; | expr ;
expr      → equality
equality  → rel equality'        equality' → (==|!=) rel equality' | ε
rel       → add rel'             rel'      → (<|<=|>|>=) add rel' | ε
add       → term add'            add'      → (+|-) term add' | ε
term      → factor term'         term'     → (*|/|%) factor term' | ε
factor    → ( expr ) | - factor | INT | FLOAT
```

验证它是无冲突 LL(1)：`./minicc3.exe -table`（退出码 0）。

## 练习

1. **加右结合幂运算 `^`**：优先级高于一元负号。需要两层新非终结符
   （`pow → factor (^ pow)?` 右递归即可——右结合不忌讳右递归）。
   改完 `grammar.c` 和 `rd_parser.c`，跑测试。
2. **故意制造 LL(1) 冲突**：把文法改成 `stmt → expr ; | expr ;`（同一 FIRST），
   或 `if` 语句带可选 else，观察 `-table` 报冲突并退出。
3. **手算验证 FIRST/FOLLOW**：对照 `-ff` 输出，验证 `FOLLOW(term') =
   FIRST(add')\{ε} ∪ FOLLOW(term)` 一类关系（龙书 4.4.2 的规则 2/3）。
4. **破坏 panic 恢复**：把 ll_parser.c 里的恢复逻辑注释掉，跑 errors.mc，
   感受"一个错误引发雪崩" vs "一次报完"的差别。

## 龙书 vs 现代

- **手写递归下降全面回归**：GCC（2004 年起弃用 yacc）、Clang、rustc、
  TypeScript、Go、V8 的 parser 全是手写递归下降 + 运算符优先级爬升
  （precedence climbing，又名 Pratt parser）。
  Part A 的循环形式就是 precedence climbing 的朴素版。
  回归原因：错误恢复与诊断质量可完全掌控、支持 IDE 增量解析、避免生成工具链。
- **LL(1) 生成器萎缩，但 LL 系复活了**：JavaCC（LL(k)）基本停在维护态；
  **ANTLR 4 用 ALL(\*)（自适应 LL，运行时按需多看）成为现代代表**；
  PEG/packrat 是另一支现代 LL 流派（解析组合子、tree-sitter 的表层语法）。
- **yacc/LALR 的兴衰**是下一课（lab4）：龙书 4.7 节的 LALR 曾统治 30 年，
  如今新项目几乎不再选它——但 LR 的思想（栈 + 状态机 + 移进归约）仍值得学透，
  因为它是理解 GLR（tree-sitter 用它做增量解析）的基础。
- 龙书 4.3 节"消除左递归/提取左因子"的机械变换**只在生成器路线里不可省**；
  手写路线用循环和优先级爬升直接绕开——又一个"理论照样学、工程有更优解"的例子。
