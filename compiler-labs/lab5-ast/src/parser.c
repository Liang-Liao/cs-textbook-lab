/*
 * parser.c —— 递归下降分析器 + 语法制导翻译（对照龙书 5.1~5.5 节）
 *
 * MiniC v4 文法（EBNF；比 lab3/lab4 多了声明/赋值/while/break/continue）：
 *
 *   program → { stmt } EOF
 *   stmt    → (int|float) IDENT [= expr] ;    ← 声明（可带初始化）
 *           | IDENT = expr ;                  ← 赋值（注意与表达式语句区分！）
 *           | print expr ;
 *           | expr ;
 *           | if ( expr ) stmt [else stmt]
 *           | while ( expr ) stmt
 *           | break ; | continue ;
 *           | { { stmt } }                    ← 块 = 新作用域
 *           | ;
 *   expr → equality {(==|!=)...}...（优先级层级与 lab3/lab4 完全一致）
 *   factor → ( expr ) | - factor | INT | FLOAT | IDENT   ← IDENT 是新加的
 *
 * 【本文件是龙书第 5 章的活体展示】语法制导翻译的三个角色一一对应：
 *   1. 综合属性（自下而上）  = 每个 parse_* 的**返回值** AstNode*。
 *      孩子先构造好，父亲把返回值组装成更大的节点——type 字段（表达式
 *      类型）就是最典型的综合属性，在构造节点的同时被检查/计算；
 *   2. 继承属性（自上而下）  = parse_stmt 的**参数** loop_depth。父节点
 *      把"你在循环里"这个上下文传给子语句，break/continue 的合法性
 *      检查完全依赖它；
 *   3. L 属性 SDD 的一遍实现（5.5 节）：整个过程单趟、自左向右、
 *      深度优先——不需要先建分析树再求属性，属性在语法分析的路上
 *      顺手就求完了。这也是 GCC/Clang 等真实编译器前端的形状。
 *
 * 【为什么不用 lab4 的 SLR(1) 表来驱动？】表驱动分析器擅长"认语法"，
 * 但做翻译时每个产生式都得挂语义动作；对有 32 个产生式、要构造 18 种
 * AST 节点的语言，手写递归下降的"代码即文法"反而最清晰——这正是
 * docs/MODERN-NOTES.md 第 5 章勘误说的"现代编译器全是手写递归下降"。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "lexer.h"

static Lexer lx;
static Token cur, nxt;   /* 双记号前瞻：stmt 层要看到 IDENT 后面是不是 '='，
                          * 才能区分赋值语句和表达式语句——比 lab3 多看 1 个 */
static int   nerr;

static void advance(void)
{
    cur = nxt;
    nxt = lexer_next_clean(&lx);
}

/* ---------------- 报错（格式与 lab1~lab4 一致：stderr + 行列定位） ---------------- */

static void syn_error(const char *what)   /* 语法错误：意外的记号 */
{
    nerr++;
    fprintf(stderr, "line %d, col %d: error: 语法错误: 意外的 %s\n",
            cur.line, cur.col, what);
}

static void sem_error(const Token *t, const char *msg)   /* 语义/类型错误 */
{
    nerr++;
    fprintf(stderr, "line %d, col %d: error: %s\n", t->line, t->col, msg);
}

static void sem_error_line(int line, const char *msg)    /* 无记号可定位时用行号 */
{
    nerr++;
    fprintf(stderr, "line %d: error: %s\n", line, msg);
}

/* 匹配终结符：对得上就前进（lab3 同款；错了不前进，让上层自然终止） */
static void match(TokenType t)
{
    if (cur.type == t) advance();
    else {
        char msg[80];
        snprintf(msg, sizeof msg, "%s（期望 %s）",
                 cur.lexeme ? cur.lexeme : "?", token_name(t));
        syn_error(msg);
    }
}

/* ---------------- 类型检查：龙书 6.1 的最小实践（为 lab6 预热） ---------------- */

static int is_compare(TokenType op)   /* 比较类运算：结果类型是 int（0/1） */
{
    return op == T_LT || op == T_LE || op == T_GT ||
           op == T_GE || op == T_EQ || op == T_NEQ;
}

/* 二元运算的语义规则（构造节点 = 求综合属性 type）：
 *   int op int   → int（/ 与 % 按C语义向零截断）
 *   任一侧 float → 先把 int 侧**包进 i2f 转换节点**，结果 float
 *   % 含 float  → 编译期错误（lab3 是运行期才发现，lab5 提前到翻译时）
 *   比较运算    → 两侧同样提升，但结果是 int
 * 在构造 AST 的同时插入 i2f，是语法制导翻译的经典应用（龙书 5.1 的
 * "类型转换"例子、6.1 的强制类型转换）：后面的阶段（lab6 生成 IR）拿
 * 到的树上转换已经显式存在，不用再各查一遍。 */
static Token op_tok;   /* 当前二元运算符的记号（parse_term 系列在 advance
                        * 前登记），供 make_binop 的语义报错精确定位——
                        * 用 cur 会指到右操作数上，偏右一格 */

static AstNode *make_binop(TokenType op, AstNode *l, AstNode *r, int line)
{
    if (l->type == TY_ERR || r->type == TY_ERR)
        return ast_binop(op, l, r, TY_ERR, line);   /* 已有错，不级联报 */

    if (op == T_PERCENT && (l->type == TY_FLOAT || r->type == TY_FLOAT)) {
        sem_error(&op_tok, "浮点数不能取模");        /* 定位到运算符本身 */
        return ast_binop(op, l, r, TY_ERR, line);
    }

    if (l->type != r->type) {                        /* 一 int 一 float：提升 */
        if (l->type == TY_INT) l = ast_i2f(l, line);
        else                   r = ast_i2f(r, line);
    }
    Type t = is_compare(op) ? TY_INT : l->type;
    return ast_binop(op, l, r, t, line);
}

/* 赋值/初始化的两侧类型要"合得上"：
 *   float ← int   ：允许，隐式把右式包进 i2f（宽度提升，C 的默认行为）
 *   int   ← float ：**禁止**（隐式收窄会丢精度；C 允许是历史包袱，
 *                   Java/Rust/Swift 都禁——见 README 练习 1 的显式转换） */
static AstNode *coerce_assign(Type target, AstNode *e, const Token *at)
{
    if (e->type == TY_ERR || e->type == target) return e;
    if (target == TY_FLOAT) return ast_i2f(e, at->line);

    char msg[96];
    snprintf(msg, sizeof msg, "不能把 float 赋给 %s（隐式收窄被禁止）",
             type_name(target));
    sem_error(at, msg);
    return e;   /* 类型仍不合，但 nerr 已记账：程序不会被执行 */
}

/* ---------------- 表达式：每个非终结符一个函数，返回 AstNode* ----------------
 * 调用链越深优先级越高：expr → equality → rel → add → term → factor（同 lab3） */

static AstNode *parse_expr(void);

static AstNode *parse_factor(void)
{
    int line = cur.line;
    if (cur.type == T_LPAREN) {          /* factor → ( expr ) */
        advance();
        AstNode *e = parse_expr();
        match(T_RPAREN);
        return e;
    }
    if (cur.type == T_MINUS) {           /* factor → - factor：类型跟随操作数 */
        advance();
        return ast_neg(parse_factor(), line);
    }
    if (cur.type == T_INT_LIT) {         /* factor → INT */
        AstNode *e = ast_int(cur.ival, line);
        advance();
        return e;
    }
    if (cur.type == T_FLOAT_LIT) {       /* factor → FLOAT */
        AstNode *e = ast_float(cur.dval, line);
        advance();
        return e;
    }
    if (cur.type == T_IDENT) {           /* factor → IDENT：变量引用。
                                          * 语法制导的关键一步——名字解析：
                                          * 编译期就把名字换成 Sym* 挂进 AST */
        Sym *sym = sym_lookup(cur.lexeme);
        if (!sym) {
            char msg[96];
            snprintf(msg, sizeof msg, "使用了未声明的变量 %s", cur.lexeme);
            sem_error(&cur, msg);
            advance();
            return ast_err(line);        /* 错误占位，防止级联报错 */
        }
        AstNode *e = ast_var(sym, line);
        advance();
        return e;
    }
    syn_error(cur.lexeme ? cur.lexeme : "<EOF>");
    advance();                           /* 错误恢复：吞掉它，分析继续 */
    return ast_err(line);
}

/* term → factor { (*|/|%) factor } —— 循环累计 = 左结合（lab3 同款） */
static AstNode *parse_term(void)
{
    AstNode *acc = parse_factor();
    while (cur.type == T_STAR || cur.type == T_SLASH || cur.type == T_PERCENT) {
        int line = cur.line;
        TokenType op = cur.type;
        op_tok = cur;                 /* 运算符定位：报错要用它自己 */
        advance();
        acc = make_binop(op, acc, parse_factor(), line);
    }
    return acc;
}

static AstNode *parse_add(void)   /* add → term { (+|-) term } */
{
    AstNode *acc = parse_term();
    while (cur.type == T_PLUS || cur.type == T_MINUS) {
        int line = cur.line;
        TokenType op = cur.type;
        advance();
        acc = make_binop(op, acc, parse_term(), line);
    }
    return acc;
}

static AstNode *parse_rel(void)   /* rel → add { (<|<=|>|>=) add } */
{
    AstNode *acc = parse_add();
    while (cur.type == T_LT || cur.type == T_LE ||
           cur.type == T_GT || cur.type == T_GE) {
        int line = cur.line;
        TokenType op = cur.type;
        advance();
        acc = make_binop(op, acc, parse_add(), line);
    }
    return acc;
}

static AstNode *parse_equality(void)   /* equality → rel { (==|!=) rel } */
{
    AstNode *acc = parse_rel();
    while (cur.type == T_EQ || cur.type == T_NEQ) {
        int line = cur.line;
        TokenType op = cur.type;
        advance();
        acc = make_binop(op, acc, parse_rel(), line);
    }
    return acc;
}

static AstNode *parse_expr(void) { return parse_equality(); }

/* ---------------- 语句 ---------------- */

static AstNode *parse_stmt(int loop_depth);

/* 声明：(int|float) IDENT [= expr] ;
 * 实现细节：**先解析初始化式，再登记符号**——所以 `int x = x;` 里的 x
 * 指向外层（或报"未声明"），而不是正在声明的这个 x。C 的规则恰好相反
 * （声明点即刻可见，`int x = x;` 是读未初始化值的 UB），C++/Java/Rust
 * 的行为与我们一致——这是作用域语义的经典分歧，见 README。 */
static AstNode *parse_decl(void)
{
    Type t = cur.type == T_KW_INT ? TY_INT : TY_FLOAT;
    advance();                            /* 吃掉 int/float */

    if (cur.type != T_IDENT) {            /* 恢复：一路吞到 ';' 为止 */
        syn_error(cur.lexeme);
        while (cur.type != T_SEMI && cur.type != T_EOF) advance();
        if (cur.type == T_SEMI) advance();
        return NULL;
    }
    Token name_tok = cur;                 /* 记住名字记号：报错定位用 */
    advance();

    AstNode *init = NULL;
    if (cur.type == T_ASSIGN) {           /* 可选初始化 */
        advance();
        init = parse_expr();
    }
    match(T_SEMI);

    Sym *sym = sym_declare(name_tok.lexeme, t, name_tok.line);
    if (!sym) {
        char msg[96];
        snprintf(msg, sizeof msg, "重复声明 %s（当前作用域已有同名符号）",
                 name_tok.lexeme);
        sem_error(&name_tok, msg);
    }
    /* 无论声明成败都按**声明的类型**收窄初值：重复声明时 AST 不带
     * 未转换节点（nerr>0 不会执行，纯防御一致性）。 */
    if (init)
        init = coerce_assign(t, init, &name_tok);
    return ast_decl(sym, init, name_tok.line);
}

/* 赋值语句：IDENT = expr ;   （进入本函数时 cur=IDENT, nxt='='） */
static AstNode *parse_assign(void)
{
    Token name_tok = cur;
    Sym *sym = sym_lookup(cur.lexeme);    /* 左值也要名字解析 */
    advance();                            /* IDENT */
    advance();                            /* '='  */

    AstNode *rhs = parse_expr();
    match(T_SEMI);

    if (!sym) {
        char msg[96];
        snprintf(msg, sizeof msg, "赋值给未声明的变量 %s", name_tok.lexeme);
        sem_error(&name_tok, msg);
        return ast_assign(NULL, rhs, name_tok.line);   /* 只留名字 */
    }
    rhs = coerce_assign(sym->type, rhs, &name_tok);
    return ast_assign(sym, rhs, name_tok.line);
}

/* 语句链（program 与 block 共用）：遇到 EOF / '}' 停止。
 * 返回链头；单个语句失败（返回 NULL）就跳过继续。 */
static AstNode *parse_stmt_list(int loop_depth, int stop_rbrace)
{
    AstNode *head = NULL, *tail = NULL;
    while (cur.type != T_EOF &&
           !(stop_rbrace && cur.type == T_RBRACE)) {
        AstNode *s = parse_stmt(loop_depth);
        if (s) {
            if (tail) tail->next = s;
            else      head = s;
            tail = s;
        }
    }
    return head;
}

/* 声明只能出现在块里。作为 if/while 的直接执行体时，名字在编译期就登记
 * 进外层作用域、运行期却要执行到才绑定——条件为假即得"编译可见、运行
 * 未绑定"的分裂局面（lab5 触发 internal error，lab6 起 -eval 更是空指针）。
 * C99 同样不允许声明作受控语句，这里对齐；for 不需要此门禁——parse_for
 * 自带 scope_push 包裹，声明落在循环自己的作用域里。 */
static AstNode *no_decl_body(AstNode *s, int line)
{
    if (s && s->kind == A_DECL)
        sem_error_line(line, "声明不能直接作为 if/while 的执行体——请用 { } 包住");
    return s;
}

static AstNode *parse_stmt(int loop_depth)
{
    int line = cur.line;
    switch (cur.type) {
    case T_KW_INT: case T_KW_FLOAT:
        return parse_decl();

    case T_KW_PRINT: {                    /* print expr ; */
        advance();
        AstNode *e = parse_expr();
        match(T_SEMI);
        return ast_print(e, line);
    }

    case T_KW_IF: {                       /* if ( expr ) stmt [else stmt] */
        advance();
        match(T_LPAREN);
        AstNode *cond = parse_expr();
        match(T_RPAREN);
        /* 条件类型 int/float 均可（C 风格"非零为真"）；Rust/Swift 强制
         * bool 的对照见 README——这是个语言设计取舍，不是编译器难题 */
        AstNode *th = no_decl_body(parse_stmt(loop_depth), line);
        AstNode *el = NULL;
        if (cur.type == T_KW_ELSE) {
            advance();
            el = no_decl_body(parse_stmt(loop_depth), line);
        }
        return ast_if(cond, th, el, line);
    }

    case T_KW_WHILE: {                    /* while ( expr ) stmt */
        advance();
        match(T_LPAREN);
        AstNode *cond = parse_expr();
        match(T_RPAREN);
        /* 继承属性下传：循环体里的 break/continue 合法，深度 +1。
         * 龙书 5.1 节 SDD 里"父节点传给子节点的上下文"在递归下降中
         * 就是一个函数参数——没有魔法。 */
        AstNode *body = no_decl_body(parse_stmt(loop_depth + 1), line);
        return ast_while(cond, body, line);
    }

    case T_KW_BREAK: case T_KW_CONTINUE: {
        const char *what = cur.type == T_KW_BREAK ? "break" : "continue";
        AstKind k = cur.type == T_KW_BREAK ? A_BREAK : A_CONTINUE;
        if (loop_depth == 0) {
            char msg[64];
            snprintf(msg, sizeof msg, "%s 在循环外", what);
            sem_error(&cur, msg);
        }
        advance();
        match(T_SEMI);
        return k == A_BREAK ? ast_break(line) : ast_continue(line);
    }

    case T_LBRACE: {                      /* { stmt_list }：块 = 新作用域 */
        advance();
        scope_push();                     /* 进块：符号表压栈（龙书 2.7） */
        AstNode *body = parse_stmt_list(loop_depth, 1);
        scope_pop();                      /* 出块：弹栈（结构保留供 -symtab） */
        match(T_RBRACE);
        return ast_block(body, line);
    }

    case T_SEMI:                          /* 空语句 */
        advance();
        return ast_empty(line);

    default:
        if (cur.type == T_IDENT && nxt.type == T_ASSIGN)
            return parse_assign();        /* 双记号前瞻在此发挥作用 */
        {                                  /* 裸表达式语句：expr ; */
            AstNode *e = parse_expr();
            match(T_SEMI);
            return ast_exprstmt(e, line);
        }
    }
}

/* ---------------- 入口 ---------------- */

AstNode *parse_program(const char *src, int *nerr_out)
{
    lexer_init(&lx, src);
    symtab_init();                        /* 全局作用域（depth 0） */
    nerr = 0;
    cur = lexer_next_clean(&lx);
    nxt = lexer_next_clean(&lx);

    for (;;) {
        AstNode *body = parse_stmt_list(0, 0);   /* 顶层：只被 EOF 终止 */
        AstNode *prog = ast_prog(body);
        if (cur.type == T_EOF) {
            if (lexer_had_error()) nerr++; /* 词法错误已由词法器报过，只计账 */
            *nerr_out = nerr;
            return prog;
        }
        /* 走到这说明顶层冒出多余的 '}'：报错吞掉，继续分析 */
        syn_error("}");
        advance();
    }
}
